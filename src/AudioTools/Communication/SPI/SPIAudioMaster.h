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
 * @brief Configuration for SPIAudioMaster
 */
struct SPIAudioMasterConfig : public AudioInfo {
#if defined(SS)
  int cs_pin = SS;
#else
  int cs_pin = 10;
#endif
  uint32_t clock_hz = 8000000;
  uint8_t bit_order = MSBFIRST;
  uint8_t data_mode = SPI_MODE0;
  size_t max_payload = 1024;
  bool is_blocking_write = false;
  void log() const {
    LOGI(
        "SPIAudioMasterConfig: sample_rate=%d, channels=%d, "
        "bits_per_sample=%d, cs_pin=%d, clock_hz=%d, bit_order=%s, "
        "data_mode=%s, max_payload=%d, is_blocking_write=%s",
        (int)sample_rate, (int)channels, (int)bits_per_sample, cs_pin, clock_hz,
        bit_order == MSBFIRST ? "MSBFIRST" : "LSBFIRST",
        data_mode == SPI_MODE0   ? "SPI_MODE0"
        : data_mode == SPI_MODE1 ? "SPI_MODE1"
        : data_mode == SPI_MODE2 ? "SPI_MODE2"
        : data_mode == SPI_MODE3 ? "SPI_MODE3"
                                 : "UNKNOWN",
        (int)max_payload, is_blocking_write ? "true" : "false");
  }
};

/**
 * @brief SPI master endpoint for remote audio sink configuration and PCM data
 * transfer.
 * @ingroup spi-audio
 *
 * This class implements the master side of a simple binary request/response
 * protocol over SPI and is designed to work with `SPIAudioSlave`.
 *
 * ## Protocol Overview
 * SPIAudioMaster provides a lightweight binary protocol for controlling a
 * remote audio sink and streaming PCM data over SPI. The protocol is
 * request/response based; the master initiates all transactions and waits for
 * slave responses.
 *
 * ## Supported Commands
 * - **SetMime**: Configure remote MIME type (e.g., "audio/wav", "audio/mp3").
 *   Helps the slave understand the audio format context.
 * - **SetAudioInfo**: Atomically set sample rate (Hz), channel count, and bits
 *   per sample in a single command. Preferred over separate format commands.
 * - **WriteData**: Stream raw PCM payload bytes to the remote input buffer.
 *   Respects remote buffer availability and configured max_payload size.
 * - **GetAvailableBufferSize**: Query free space in the remote buffer (bytes).
 *   Enables flow control to prevent remote buffer overflow.
 * - **GetFilledBufferSize**: Query how many bytes are currently buffered on the
 *   remote side and ready to be consumed. Useful for monitoring latency.
 * - **Clear**: Reset the remote buffer (useful for state recovery).
 *
 * ## Wire Format
 * **Request frame**: `cmd(1) + payload_len(2 LE) + payload(0..65535)`
 * **Response frame**: `status(1) + response_len(2 LE) + response(0..65535)`
 *
 * All multi-byte integers are little-endian. Payload length is uint16_t,
 * allowing up to 64KB payloads per command.
 *
 * ## Flow Control
 * The master supports two write modes via `is_blocking_write`:
 * - **is_blocking_write=false** (default): Non-blocking writes. `write()` sends
 * only the bytes that currently fit into the remote buffer and returns
 * immediately.
 * - **is_blocking_write=true**: Blocking writes. `write()` waits until enough
 * remote space is available for one chunk and then sends that chunk in a single
 *   SPI command.
 *
 * ## Status Codes
 * Responses include status byte:
 * - 0: Ok (command executed successfully)
 * - 1: InvalidCommand (slave doesn't recognize command)
 * - 2: InvalidPayload (payload length or format incorrect)
 * - 3: BufferOverflow (slave buffer full, write rejected)
 * - 4: InternalError (slave-side error)
 *
 * ## SPI Configuration
 * Configure SPI bus settings via `SPIAudioMasterConfig`:
 * - `cs_pin`: Chip select GPIO (defaults to SS if defined, else 10). Set to
 *   `-1` to disable manual CS handling (external or hardware-controlled CS).
 * - `clock_hz`: SPI clock frequency (default 8 MHz)
 * - `bit_order`: MSBFIRST or LSBFIRST (default MSBFIRST)
 * - `data_mode`: SPI_MODE0..SPI_MODE3 (default SPI_MODE0)
 * - `max_payload`: Max bytes per WriteData command (default 1024)
 * - `is_blocking_write`: Enable blocking write mode (default false)
 *
 * ## Typical Usage
 * ```cpp
 * SPIAudioMaster master;
 * SPIAudioMasterConfig cfg;
 * cfg.sample_rate = 48000;
 * cfg.channels = 2;
 * cfg.bits_per_sample = 16;
 * cfg.clock_hz = 10000000;  // 10 MHz SPI clock
 * master.begin(cfg);
 *
 * master.setMime("audio/wav");
 * master.setAudioInfo(cfg);  // Sync format to slave
 *
 * // Stream audio data
 * while (audio_available) {
 *     size_t sent = master.write(audio_buffer, chunk_size);
 * }
 *
 * master.end();
 * ```
 *
 * ## Performance Notes
 * - Each command triggers a full SPI transaction (multiple CS toggles).
 * - Minimize SetAudioInfo/SetMime calls after initialization.
 * - Use large max_payload to reduce command overhead (if slave buffer allows).
 * - Batch small audio chunks to fill max_payload per WriteData command.
 */
class SPIAudioMaster : public AudioOutput {
 public:
  /// Default constructor.
  SPIAudioMaster() = default;
  /// Constructor with explicit SPI bus instance.
  explicit SPIAudioMaster(SPIClass& spi) : p_spi(&spi) {}

  /// Initializes SPI and stores the master configuration.
  bool begin(SPIAudioMasterConfig cfg = SPIAudioMasterConfig()) {
    config = cfg;
    config.log();
    if (p_spi == nullptr) {
      p_spi = &SPI;
    }
    if (config.cs_pin >= 0) {
      pinMode(config.cs_pin, OUTPUT);
      digitalWrite(config.cs_pin, HIGH);
    }
    p_spi->begin();
    active = true;

    clear();  // Clear remote buffer on start
    buffer_size = getAvailableBufferSize();
    LOGI("SPIAudioMaster started with remote buffer size: %d bytes",
         buffer_size);
    return true;
  }

  /// Stops the master and closes the SPI bus.
  void end() {
    clear();
    if (p_spi != nullptr) {
      p_spi->end();
    }
    active = false;
  }

  /// Returns true when the master is active.
  bool isActive() const { return active; }

  /// Set stream mime type (e.g. "audio/wav", "audio/mp3")
  bool setMime(const char* mime) {
    if (mime == nullptr) return false;
    return sendCommand(SPIAudioCommand::SetMime,
                       reinterpret_cast<const uint8_t*>(mime), strlen(mime) + 1,
                       nullptr, nullptr);
  }

  /// Sends audio format information to the remote slave.
  void setAudioInfo(AudioInfo info) override {
    AudioOutput::setAudioInfo(info);
    uint8_t payload[6];
    writeU32LE(payload, info.sample_rate);
    payload[4] = static_cast<uint8_t>(info.channels);
    payload[5] = static_cast<uint8_t>(info.bits_per_sample);
    (void)sendCommand(SPIAudioCommand::SetAudioInfo, payload, sizeof(payload),
                      nullptr, nullptr);
  }

  /// Writes audio bytes in chunks using WriteData command
  size_t write(const uint8_t* data, size_t len) {
    if (!active) return 0;
    if (data == nullptr || len == 0) return 0;
    if (len > buffer_size) {
      LOGE("Attempting to write %d bytes, but remote buffer size is only %d",
           (int)len, buffer_size);
      return 0;
    }

    size_t chunk = min(config.max_payload, len);
    if (chunk == 0) return 0;

    if (!config.is_blocking_write) {
      size_t available = getAvailableBufferSize();
      chunk = min(chunk, available);
      if (chunk == 0) return 0;
      return sendCommand(SPIAudioCommand::WriteData, data, chunk, nullptr,
                         nullptr)
                 ? chunk
                 : 0;
    }

    // wait until enough space is available for the chunk
    while (getAvailableBufferSize() < chunk) {
      delay(10);
    }
    return sendCommand(SPIAudioCommand::WriteData, data, chunk, nullptr,
                       nullptr)
               ? chunk
               : 0;
  }

  /// Returns writable bytes on the remote side.
  int availableForWrite() {
    return active ? static_cast<int>(getAvailableBufferSize()) : 0;
  }

  /// Clears the remote audio buffer
  bool clear() {
    return sendCommand(SPIAudioCommand::Clear, nullptr, 0, nullptr, nullptr);
  }

  /// Queries how many bytes are currently filled/readable in the remote buffer
  int available() {
    return active ? static_cast<int>(getFilledBufferSize()) : 0;
  }

 protected:
  SPIAudioMasterConfig config;
  bool active = false;
  SPIClass* p_spi = nullptr;
  int buffer_size = 0;

  /// Queries available space in the remote buffer in bytes
  uint32_t getAvailableBufferSize() {
    uint8_t response[4] = {0, 0, 0, 0};
    uint16_t response_len = sizeof(response);
    if (!sendCommand(SPIAudioCommand::GetAvailableBufferSize, nullptr, 0,
                     response, &response_len)) {
      return 0;
    }
    if (response_len < 4) {
      return 0;
    }
    return readU32LE(response);
  }

  /// Queries how many bytes are filled/readable in the remote buffer
  uint32_t getFilledBufferSize() {
    uint8_t response[4] = {0, 0, 0, 0};
    uint16_t response_len = sizeof(response);
    if (!sendCommand(SPIAudioCommand::GetFilledBufferSize, nullptr, 0, response,
                     &response_len)) {
      return 0;
    }
    if (response_len < 4) {
      return 0;
    }
    return readU32LE(response);
  }

  /// Sends a protocol command and reads the optional response.
  bool sendCommand(SPIAudioCommand command, const uint8_t* payload,
                   uint16_t payload_len, uint8_t* response,
                   uint16_t* response_len) {
    if (!active || p_spi == nullptr) return false;

    SPISettings settings(config.clock_hz, config.bit_order, config.data_mode);
    p_spi->beginTransaction(settings);
    if (config.cs_pin >= 0) {
      digitalWrite(config.cs_pin, LOW);
    }

    p_spi->transfer(static_cast<uint8_t>(command));
    transferU16LE(payload_len);

    for (uint16_t i = 0; i < payload_len; i++) {
      p_spi->transfer(payload != nullptr ? payload[i] : 0);
    }

    uint8_t status = p_spi->transfer(0x00);
    uint16_t len = transferU16LE(0);
    uint16_t response_capacity = 0;

    if (response != nullptr && response_len != nullptr) {
      response_capacity = *response_len;
    }

    if (response_len != nullptr) {
      *response_len = len;
    }

    for (uint16_t i = 0; i < len; i++) {
      uint8_t value = p_spi->transfer(0x00);
      if (response != nullptr && i < response_capacity) {
        response[i] = value;
      }
    }

    if (config.cs_pin >= 0) {
      digitalWrite(config.cs_pin, HIGH);
    }
    p_spi->endTransaction();

    return status == 0;
  }

  /// Transfers and/or receives a 16-bit little-endian value over SPI.
  uint16_t transferU16LE(uint16_t value) {
    uint8_t lo = p_spi->transfer(static_cast<uint8_t>(value & 0xFF));
    uint8_t hi = p_spi->transfer(static_cast<uint8_t>((value >> 8) & 0xFF));
    return static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8);
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