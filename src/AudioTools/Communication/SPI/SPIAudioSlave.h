#pragma once

#include <SPI.h>

#include "AudioTools/CoreAudio/AudioBasic/Str.h"
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "SPIAudioCommand.h"

#if defined(ESP32)
#include "driver/spi_slave.h"
#endif

namespace audio_tools {

/**
 * @brief Configuration for SPIAudioSlave
 *
 * ## Performance Tuning (ESP32)
 * For optimal performance with ESP32 SPI slave hardware:
 * - **queue_size**: Increase from default 4 to 8+ if experiencing buffer
 *   overflow or if audio data arrives in large bursts. Each queue entry
 *   consumes ~256 bytes of DMA-capable RAM. Start with 4 and increase as
 * needed.
 * - **dma_chan**: DMA channel (1 or 2). Usually doesn't affect performance.
 * - **use_hw_slave**: Set to true (default) to use hardware SPI slave driver
 *   instead of bit-banging, significantly improving throughput.
 */
struct SPIAudioSlaveConfig : public AudioInfo {
  size_t max_command_payload = 1024;
  size_t max_response_payload = 32;
  size_t max_mime_len = 20;
#if defined(ESP32)
  bool use_hw_slave = true;
#if defined(SPI2_HOST)
  spi_host_device_t host = SPI2_HOST;
#elif defined(HSPI_HOST)
  spi_host_device_t host = HSPI_HOST;
#elif defined(SPI3_HOST)
  spi_host_device_t host = SPI3_HOST;
#elif defined(VSPI_HOST)
  spi_host_device_t host = VSPI_HOST;
#else
  spi_host_device_t host = SPI2_HOST;
#endif
  int dma_chan = 1;
  int pin_mosi = 23;
  int pin_miso = 19;
  int pin_sclk = 18;
  int pin_cs = 5;
  /// Queue size for SPI slave transactions. Higher values improve throughput
  /// at the cost of DMA RAM. Default 4 balances performance and memory usage.
  int queue_size = 4;
#endif
  void log() const {
    LOGI(
        "SPIAudioSlaveConfig: sample_rate=%d, channels=%d, bits_per_sample=%d, "
        "max_command_payload=%d, max_response_payload=%d, max_mime_len=%d ",
        (int)sample_rate, (int)channels, (int)bits_per_sample,
        (int)max_command_payload, (int)max_response_payload, (int)max_mime_len);
#if defined(ESP32)
    LOGI(
        "ESP32 SPI Slave Config: use_hw_slave=%s, host=%d, dma_chan=%d, "
        "pin_mosi=%d, pin_miso=%d, pin_sclk=%d, pin_cs=%d, queue_size=%d",
        use_hw_slave ? "true" : "false", host, dma_chan, pin_mosi, pin_miso,
        pin_sclk, pin_cs, queue_size);
#endif
  }
};

/**
 * @brief SPI Audio Slave endpoint matching SPIAudioMaster protocol.
 * @ingroup spi-audio
 *
 * This class implements the slave side of a binary request/response protocol
 * over SPI. It receives commands from a remote SPIAudioMaster and manages an
 * externally-provided audio buffer.
 *
 * ## Protocol Role
 * SPIAudioSlave responds passively to SPI transactions initiated by the
 * master. It parses incoming command frames, executes commands, and sends
 * response frames back. The slave does not control the SPI bus clock or
 * timing.
 *
 * ## Supported Commands
 * - **SetMime** (1): Store MIME type string for format context
 * - **SetAudioInfo** (2): Accept audio format (sample rate, channels,
 * bits/sample)
 * - **WriteData** (5): Accept PCM bytes into the managed audio buffer
 * - **GetAvailableBufferSize** (6): Report free space in audio buffer
 * - **Clear** (7): Reset the audio buffer to empty state
 * - **GetFilledBufferSize** (8): Report bytes currently buffered and readable
 *
 * ## Buffer Management
 * The slave requires an externally-provided `BaseBuffer<uint8_t>` passed via
 * constructor. The slave does NOT manage buffer memory or sizing; the caller
 * is responsible for:
 * - Creating the buffer (e.g., RingBuffer, Vector)
 * - Sizing it appropriately for your audio latency requirements
 * - Reading audio bytes via `readBytes()` on your schedule
 * - Managing the buffer lifecycle
 *
 * This design decouples protocol handling from buffer implementation,
 * allowing flexible buffer strategies (ring, linear, DMA-based, etc.).
 *
 * ## Protocol Processing Modes
 * The slave supports two processing patterns:
 *
 * ### 1. Byte-wise Handler (Flexible)
 * Call `onTransfer(byte)` for each SPI byte received (e.g., from an ISR):
 * ```cpp
 * SPIAudioSlave slave(my_buffer);
 * slave.begin(cfg);
 *
 * void spi_isr() {
 *   uint8_t rx_byte = SPI.transfer(0);
 *   uint8_t tx_byte = slave.onTransfer(rx_byte);
 *   SPI.transfer(tx_byte);
 * }
 * ```
 * Returns the response byte to send back on the next SPI clock cycle.
 *
 * ### 2. Frame-based Helper (Testing/Integration)
 * Use `processFrame()` to handle complete request/response frames at once:
 * ```cpp
 * uint8_t request[256], response[256];
 * uint16_t response_len;
 * slave.processFrame(request, req_len, response, sizeof(response),
 * &response_len);
 * ```
 * Useful for unit tests or SPI drivers that buffer complete transactions.
 *
 * ### 3. ESP32 Hardware Slave Mode (Optional)
 * On ESP32 with `use_hw_slave=true`, call `process()` in your main loop:
 * ```cpp
 * while (1) {
 *   slave.process();  // Performs one byte transaction with HW SPI slave
 * }
 * ```
 * The hardware SPI slave driver handles all clock/timing; `process()` just
 * feeds bytes to `onTransfer()`.
 *
 * ## State Machine
 * The slave uses an 8-state machine for parsing request/response frames:
 * - **RxCmd**: Receive command byte, initialize request
 * - **RxLenLo**: Receive payload length (low byte)
 * - **RxLenHi**: Receive payload length (high byte)
 * - **RxPayload**: Receive variable-length payload bytes
 * - **TxStatus**: Transmit status code (0 = ok, 1-4 = error)
 * - **TxLenLo**: Transmit response length (low byte)
 * - **TxLenHi**: Transmit response length (high byte)
 * - **TxPayload**: Transmit variable-length response bytes
 *
 * After completing a response, the state machine loops back to RxCmd.
 *
 * ## Audio Data Access
 * After the slave has accepted audio via WriteData commands, retrieve it
 * using:
 * - `available()`: Returns bytes ready to read
 * - `readBytes(buf, len)`: Read up to len bytes into buf
 * - `availableForWrite()`: Check space before master sends more data
 * - `write(buf, len)`: Directly write to buffer (for local testing)
 * - `clearBuffer()`: Empty the buffer
 *
 * ## Error Handling
 * Commands may fail with status codes (see `StatusCode` enum):
 * - **Ok** (0): Success
 * - **InvalidCommand** (1): Command ID not recognized
 * - **InvalidPayload** (2): Payload length or format incorrect
 * - **BufferOverflow** (3): WriteData rejected; buffer full
 * - **InternalError** (4): Slave resource error
 *
 * ## ESP32-Specific Configuration
 * When `use_hw_slave=true` (default on ESP32):
 * - **host**: SPI host (SPI2_HOST/HSPI_HOST/VSPI_HOST)
 * - **dma_chan**: DMA channel (1 or 2)
 * - **pin_mosi/miso/sclk/cs**: GPIO pin assignments
 * - **queue_size**: Transaction queue depth (default 4)
 *
 * ## Typical Usage
 * ```cpp
 * // Create a ring buffer for incoming audio
 * RingBuffer<uint8_t> audio_buffer(48000 * 2);  // 1 second @ 48kHz stereo
 * 16-bit
 *
 * SPIAudioSlave slave(audio_buffer);
 * SPIAudioSlaveConfig cfg;
 * cfg.sample_rate = 48000;
 * cfg.channels = 2;
 * cfg.bits_per_sample = 16;
 * slave.begin(cfg);
 *
 * // Option A: Byte-wise in SPI ISR
 * ISR_HANDLER void spi_isr() {
 *   uint8_t rx = read_spi();
 *   uint8_t tx = slave.onTransfer(rx);
 *   write_spi(tx);
 * }
 *
 * // Option B: Frame-based polling
 * while (1) {
 *   if (spi_has_frame()) {
 *     slave.processFrame(request, req_len, response, resp_max, &resp_len);
 *   }
 *   if (slave.available() > 0) {
 *     slave.readBytes(audio_chunk, chunk_size);
 *     play_audio(audio_chunk, chunk_size);
 *   }
 * }
 *
 * // Option C: ESP32 hardware SPI slave (most efficient)
 * while (1) {
 *   slave.process();  // One byte transaction
 *   if (slave.available() > threshold) {
 *     slave.readBytes(audio_chunk, chunk_size);
 *     play_audio(audio_chunk, chunk_size);
 *   }
 * }
 *
 * slave.end();
 * ```
 *
 * ## Performance Notes
 * - **Byte-wise handler**: Minimal latency, suitable for ISR context. No
 * buffering.
 * - **Frame-based**: Simpler integration, good for testing. Requires
 * reassembly.
 * - **ESP32 hardware**: Highest throughput with minimal CPU load via DMA.
 * - **Buffer sizing**: Undersize = frequent buffer overflow, Oversize =
 * excess latency.
 * - **Read frequency**: Poll `available()` regularly to prevent overflow.
 */
class SPIAudioSlave : public AudioStream {
 public:
  enum StatusCode : uint8_t {
    Ok = 0,
    InvalidCommand = 1,
    InvalidPayload = 2,
    BufferOverflow = 3,
    InternalError = 4
  };

  /// Constructor using an externally provided buffer implementation.
  SPIAudioSlave(BaseBuffer<uint8_t>& buffer) : p_buffer(&buffer) {}

  /// Initializes protocol state, buffers and optional HW slave support.
  bool begin(SPIAudioSlaveConfig cfg = SPIAudioSlaveConfig()) {
    config = cfg;
    config.log();
    setAudioInfo(cfg);
    if (config.max_command_payload == 0) config.max_command_payload = 1;
    if (config.max_response_payload == 0) config.max_response_payload = 1;
    if (config.max_mime_len == 0) config.max_mime_len = 1;

    if (p_buffer == nullptr) return false;
    p_buffer->reset();
    request_payload.resize(config.max_command_payload);
    response_payload.resize(config.max_response_payload);
    mime.setCapacity(config.max_mime_len > 0 ? config.max_mime_len - 1 : 0);
    mime = "";

    resetProtocolState();

#if defined(ESP32)
    if (config.use_hw_slave) {
      spi_bus_config_t bus_cfg;
      memset(&bus_cfg, 0, sizeof(bus_cfg));
      bus_cfg.mosi_io_num = config.pin_mosi;
      bus_cfg.miso_io_num = config.pin_miso;
      bus_cfg.sclk_io_num = config.pin_sclk;
      bus_cfg.quadwp_io_num = -1;
      bus_cfg.quadhd_io_num = -1;

      spi_slave_interface_config_t slave_cfg;
      memset(&slave_cfg, 0, sizeof(slave_cfg));
      slave_cfg.mode = 0;
      slave_cfg.spics_io_num = config.pin_cs;
      slave_cfg.queue_size = config.queue_size;
      slave_cfg.flags = 0;

      esp_err_t rc = spi_slave_initialize(config.host, &bus_cfg, &slave_cfg,
                                          config.dma_chan);
      if (rc != ESP_OK) {
        active = false;
        return false;
      }
      esp32_spi_active = true;
      spi_tx_byte = 0;
    }
#endif

    active = true;
    return true;
  }

  /// Stops the slave and releases all resources.
  void end() override {
#if defined(ESP32)
    if (esp32_spi_active) {
      spi_slave_free(config.host);
      esp32_spi_active = false;
    }
#endif
    active = false;
    if (p_buffer != nullptr) p_buffer->reset();
    resetProtocolState();
  }

  /// Returns true when the slave is active.
  bool isActive() const { return active; }

  /// Updates current audio format and mirrors it into config.
  void setAudioInfo(AudioInfo info) override {
    AudioStream::setAudioInfo(info);
    config.copyFrom(info);
  }

  /// Processes one hardware SPI byte transaction when HW slave mode is active
  /// and forwards it through `onTransfer()`. Returns false when no byte was
  /// processed.
  bool process() {
#if defined(ESP32)
    if (!esp32_spi_active) return false;

    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &spi_tx_byte;
    t.rx_buffer = &spi_rx_byte;

    esp_err_t rc = spi_slave_transmit(config.host, &t, 0);
    if (rc != ESP_OK) return false;

    spi_tx_byte = onTransfer(spi_rx_byte);
    // If the last Rx byte triggered a Rx→TxStatus transition, the returned
    // value was 0 (Rx state return). Pre-advance through TxStatus now so that
    // spi_tx_byte holds tx_status before the next spi_slave_transmit call,
    // eliminating the 1-byte pipeline shift in the response stream.
    if (state == TxStatus) {
      spi_tx_byte = onTransfer(0);
    }
    return true;
#else
    return false;
#endif
  }

  /// Byte-wise SPI handler. Call this for each received SPI byte.
  uint8_t onTransfer(uint8_t in) {
    switch (state) {
      case RxCmd:
        current_cmd = in;
        expected_payload_len = 0;
        rx_pos = 0;
        tx_pos = 0;
        tx_len = 0;
        tx_status = Ok;
        state = RxLenLo;
        return 0;

      case RxLenLo:
        expected_payload_len = in;
        state = RxLenHi;
        return 0;

      case RxLenHi:
        expected_payload_len |= static_cast<uint16_t>(in) << 8;
        if (expected_payload_len == 0) {
          prepareResponse();
          state = TxStatus;
        } else {
          rx_pos = 0;
          state = RxPayload;
        }
        return 0;

      case RxPayload:
        if (rx_pos < request_payload.size()) {
          request_payload[rx_pos] = in;
        }
        rx_pos++;
        if (rx_pos >= expected_payload_len) {
          prepareResponse();
          state = TxStatus;
        }
        return 0;

      case TxStatus:
        state = TxLenLo;
        return tx_status;

      case TxLenLo:
        state = TxLenHi;
        return static_cast<uint8_t>(tx_len & 0xFF);

      case TxLenHi: {
        uint8_t out = static_cast<uint8_t>((tx_len >> 8) & 0xFF);
        if (tx_len == 0) {
          state = RxCmd;
        } else {
          state = TxPayload;
        }
        return out;
      }

      case TxPayload: {
        uint8_t out = tx_pos < tx_len ? response_payload[tx_pos] : 0;
        tx_pos++;
        if (tx_pos >= tx_len) {
          state = RxCmd;
        }
        return out;
      }
    }
    resetProtocolState();
    return 0;
  }

  /// Frame-based helper (same wire format as master, useful for tests)
  bool processFrame(const uint8_t* request_frame, uint16_t request_frame_len,
                    uint8_t* response_frame, uint16_t response_frame_max,
                    uint16_t* response_frame_len = nullptr) {
    if (request_frame == nullptr || request_frame_len < 3 ||
        response_frame == nullptr || response_frame_max < 3) {
      return false;
    }

    uint8_t cmd = request_frame[0];
    uint16_t payload_len = static_cast<uint16_t>(request_frame[1]) |
                           (static_cast<uint16_t>(request_frame[2]) << 8);
    if (static_cast<uint32_t>(payload_len) + 3 > request_frame_len) {
      return false;
    }

    uint16_t out_len = 0;
    uint8_t status = processCommand(
        static_cast<SPIAudioCommand>(cmd), request_frame + 3, payload_len,
        response_frame + 3, response_frame_max - 3, out_len);

    response_frame[0] = status;
    response_frame[1] = static_cast<uint8_t>(out_len & 0xFF);
    response_frame[2] = static_cast<uint8_t>((out_len >> 8) & 0xFF);

    if (response_frame_len != nullptr) {
      *response_frame_len = out_len + 3;
    }
    return true;
  }

  // Audio data access
  /// Returns readable bytes currently buffered.
  int available() override {
    return p_buffer == nullptr ? 0 : p_buffer->available();
  }

  /// Returns remaining writable bytes in the input buffer.
  int availableForWrite() override {
    return p_buffer == nullptr ? 0 : p_buffer->availableForWrite();
  }

  /// Reads buffered audio bytes.
  size_t readBytes(uint8_t* data, size_t len) override {
    if (!active || data == nullptr || len == 0) return 0;
    if (p_buffer == nullptr) return 0;
    int to_read = min(static_cast<int>(len), p_buffer->available());
    if (to_read <= 0) return 0;
    return p_buffer->readArray(data, to_read);
  }

  /// Writes bytes directly into the local slave buffer.
  size_t write(const uint8_t* data, size_t len) override {
    if (!active || data == nullptr || len == 0) return 0;
    if (p_buffer == nullptr) return 0;
    int to_write = min(static_cast<int>(len), p_buffer->availableForWrite());
    if (to_write <= 0) return 0;
    return p_buffer->writeArray(data, to_write);
  }

  /// Clears local buffered audio data.
  void clearBuffer() {
    if (p_buffer != nullptr) p_buffer->reset();
  }

  /// Returns the currently configured mime type.
  const char* mimeType() { return mime.c_str(); }

 protected:
  enum ProtocolState : uint8_t {
    RxCmd,
    RxLenLo,
    RxLenHi,
    RxPayload,
    TxStatus,
    TxLenLo,
    TxLenHi,
    TxPayload
  };

  SPIAudioSlaveConfig config;
  bool active = false;
  BaseBuffer<uint8_t>* p_buffer = nullptr;
  Vector<uint8_t> request_payload{0};
  Vector<uint8_t> response_payload{0};
  Str mime;

#if defined(ESP32)
  bool esp32_spi_active = false;
  uint8_t spi_tx_byte = 0;
  uint8_t spi_rx_byte = 0;
#endif

  // protocol state
  ProtocolState state = RxCmd;
  uint8_t current_cmd = 0;
  uint16_t expected_payload_len = 0;
  uint16_t rx_pos = 0;
  uint16_t tx_pos = 0;
  uint16_t tx_len = 0;
  uint8_t tx_status = Ok;

  /// Resets internal request/response parser state.
  void resetProtocolState() {
    state = RxCmd;
    current_cmd = 0;
    expected_payload_len = 0;
    rx_pos = 0;
    tx_pos = 0;
    tx_len = 0;
    tx_status = Ok;
  }

  /// Evaluates current request payload and prepares response payload.
  void prepareResponse() {
    if (expected_payload_len > request_payload.size()) {
      tx_status = InvalidPayload;
      tx_len = 0;
      tx_pos = 0;
      return;
    }

    uint16_t out_len = 0;
    tx_status = processCommand(
        static_cast<SPIAudioCommand>(current_cmd), request_payload.data(),
        expected_payload_len, response_payload.data(),
        static_cast<uint16_t>(response_payload.size()), out_len);
    tx_len = out_len;
    tx_pos = 0;
  }

  /// Executes one protocol command and writes response payload.
  uint8_t processCommand(SPIAudioCommand cmd, const uint8_t* payload,
                         uint16_t payload_len, uint8_t* response,
                         uint16_t response_max, uint16_t& response_len) {
    response_len = 0;

    switch (cmd) {
      case SPIAudioCommand::SetMime: {
        if (payload_len == 0 || payload == nullptr) return InvalidPayload;
        size_t max_len = config.max_mime_len > 0 ? config.max_mime_len - 1 : 0;
        size_t n = min(static_cast<size_t>(payload_len), max_len);
        if (n > 0 && payload[n - 1] == '\0') {
          --n;
        }
        mime.copyFrom(reinterpret_cast<const char*>(payload), n, max_len);
        return Ok;
      }

      case SPIAudioCommand::SetAudioInfo: {
        if (payload_len != 6 || payload == nullptr) return InvalidPayload;
        AudioInfo info = audioInfo();
        info.sample_rate = readU32LE(payload);
        info.channels = payload[4];
        info.bits_per_sample = payload[5];
        setAudioInfo(info);
        return Ok;
      }

      case SPIAudioCommand::WriteData: {
        if (payload_len == 0 || payload == nullptr) return Ok;
        if (p_buffer == nullptr) return InternalError;
        int written = p_buffer->writeArray(payload, payload_len);
        return written == static_cast<int>(payload_len) ? Ok : BufferOverflow;
      }

      case SPIAudioCommand::GetAvailableBufferSize: {
        if (response == nullptr || response_max < 4) return InternalError;
        if (p_buffer == nullptr) return InternalError;
        int avail = p_buffer->availableForWrite();
        writeU32LE(response, static_cast<uint32_t>(avail > 0 ? avail : 0));
        response_len = 4;
        return Ok;
      }

      case SPIAudioCommand::Clear: {
        if (p_buffer != nullptr) p_buffer->reset();
        return Ok;
      }

      case SPIAudioCommand::GetFilledBufferSize: {
        if (response == nullptr || response_max < 4) return InternalError;
        if (p_buffer == nullptr) return InternalError;
        int filled = p_buffer->available();
        writeU32LE(response, static_cast<uint32_t>(filled > 0 ? filled : 0));
        response_len = 4;
        return Ok;
      }

      default:
        return InvalidCommand;
    }
  }

  /// Writes a 32-bit little-endian integer to the target buffer.
  static void writeU32LE(uint8_t* out, uint32_t value) {
    out[0] = static_cast<uint8_t>(value & 0xFF);
    out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    out[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    out[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
  }

  /// Reads a 32-bit little-endian integer from the source buffer.
  static uint32_t readU32LE(const uint8_t* in) {
    return static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) |
           (static_cast<uint32_t>(in[2]) << 16) |
           (static_cast<uint32_t>(in[3]) << 24);
  }
};

}  // namespace audio_tools