#pragma once

#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "SPIRegAudioRegisters.h"
#include "TinySPISlave.h"

namespace audio_tools {

/**
 * @brief Configuration for SPIRegAudioSlave
 * @ingroup spi-audio
 */
struct SPIRegAudioSlaveConfig : public AudioInfo {
  int sck = -1;
  int miso = -1;
  int mosi = -1;
  int cs = -1;
  uint8_t spi_mode = 0;
  /// Largest number of audio bytes the master is allowed to send in one frame
  size_t max_payload = 256;
  /// ESP32 only: SPI host (2 = SPI2, 3 = SPI3)
  uint8_t spi_host = 2;
};

/**
 * @brief SPI audio slave based on the TinySPISlave library: the master
 * controls us via a small register map (see SPIRegAudioRegisters.h):
 * - SPIREG_AUDIO_INFO: sample rate, channels and bits per sample
 * - SPIREG_MIME: mime type of the audio data
 * - SPIREG_AVAILABLE: free space in our buffer (the master uses it for flow
 *   control)
 * - SPIREG_DATA: the audio data
 *
 * The received audio is stored in the provided buffer; read it with
 * readBytes() or copy it to an output with StreamCopy. Call update() from
 * loop(): it applies format changes (AudioInfo notifications) in the
 * application context. The SPI frames themselves are processed in the
 * interrupt (RP2040/STM32) or in the TinySPISlave task (ESP32), so the buffer
 * should be safe for one writer (SPI) and one reader (loop), e.g. a
 * RingBuffer.
 *
 * @note Requires the TinySPISlave library (ESP32, RP2040/RP2350, STM32).
 * @code
 * RingBuffer<uint8_t> buffer(8 * 1024);
 * SPIRegAudioSlave slave(buffer);
 * auto cfg = SPIRegAudioSlaveConfig();
 * cfg.sck = 18; cfg.miso = 19; cfg.mosi = 23; cfg.cs = 5;
 * slave.begin(cfg);
 * slave.addNotifyAudioChange(i2s);  // format is set by the master
 * StreamCopy copier(i2s, slave);
 * void loop() { slave.update(); copier.copy(); }
 * @endcode
 */
class SPIRegAudioSlave : public AudioStream, public MimeSource {
 public:
  SPIRegAudioSlave(BaseBuffer<uint8_t>& buffer) : p_buffer(&buffer) {}

  bool begin(SPIRegAudioSlaveConfig cfg = SPIRegAudioSlaveConfig()) {
    end();
    config = cfg;
    if (p_buffer == nullptr || config.max_payload == 0) return false;
    p_buffer->reset();
    memset(info_raw, 0, sizeof(info_raw));
    memset(mime_raw, 0, sizeof(mime_raw));
    memset(mime_str, 0, sizeof(mime_str));
    pending_info = false;
    pending_mime = false;
    overflow_bytes = 0;
    if (cfg) {
      encodeInfo(cfg, info_raw);
      AudioStream::setAudioInfo(cfg);
    }

    tinyspi::Config sc;
    sc.sck = config.sck;
    sc.miso = config.miso;
    sc.mosi = config.mosi;
    sc.cs = config.cs;
    sc.mode = config.spi_mode;
    sc.rxBufferSize = 2 * (config.max_payload + 2);
    sc.txBufferSize = 64;
    // a frame must not be split: [cmd] + payload
    sc.maxFrameSize = config.max_payload + 2;
    sc.slots = 1;  // reply is clocked out in the very next frame
    sc.spiHost = config.spi_host;
    if (!spi.begin(sc)) {
      LOGE("SPIRegAudioSlave: TinySPISlave begin failed");
      return false;
    }
    spi.onFrameImmediate(&SPIRegAudioSlave::frameThunk, this);
    active = true;
    return true;
  }

  void end() override {
    if (active) {
      spi.onFrameImmediate(nullptr, nullptr);
      spi.end();
    }
    active = false;
  }

  bool isActive() const { return active; }

  /// Call from loop(): applies AudioInfo / mime changes made by the master
  void update() {
    if (!active) return;
    if (pending_mime) {
      char tmp[SPIREG_MIME_SIZE];
      {
        tinyspi::detail::Critical c;
        memcpy(tmp, mime_raw, sizeof(tmp));
        pending_mime = false;
      }
      tmp[sizeof(tmp) - 1] = 0;
      LOGI("SPIRegAudioSlave mime: %s", tmp);
      memcpy(mime_str, tmp, sizeof(mime_str));
      if (mime_cb) mime_cb(mime_str, mime_ctx);
    }
    if (pending_info) {
      uint8_t tmp[SPIREG_AUDIO_INFO_SIZE];
      {
        tinyspi::detail::Critical c;
        memcpy(tmp, info_raw, sizeof(tmp));
        pending_info = false;
      }
      AudioInfo new_info;
      decodeInfo(tmp, new_info);
      setAudioInfo(new_info);
    }
  }

  /// Mime type defined by the master (nullptr if not set): this allows to use
  /// the slave as source for the MultiDecoder
  const char* mime() override { return mime_str[0] ? mime_str : nullptr; }

  /// Mime type defined by the master (empty if not set)
  const char* mimeType() { return mime_str; }

  /// Callback which is called from update() when the master has defined a
  /// (new) mime type: e.g. to restart the decoder
  void setMimeCallback(void (*cb)(const char* mime, void* ctx),
                       void* ctx = nullptr) {
    mime_cb = cb;
    mime_ctx = ctx;
  }

  /// Readable audio bytes
  int available() override {
    return p_buffer == nullptr ? 0 : p_buffer->available();
  }

  /// Free space in the buffer: this is the value provided to the master
  int availableForWrite() override {
    return p_buffer == nullptr ? 0 : p_buffer->availableForWrite();
  }

  size_t readBytes(uint8_t* data, size_t len) override {
    if (!active || data == nullptr || len == 0 || p_buffer == nullptr) return 0;
    int n = min((int)len, p_buffer->available());
    return n > 0 ? p_buffer->readArray(data, n) : 0;
  }

  /// Number of audio bytes dropped because the master ignored the flow control
  uint32_t overflowBytes() const { return overflow_bytes; }

 protected:
  SPIRegAudioSlaveConfig config;
  TinySPISlave spi;
  BaseBuffer<uint8_t>* p_buffer = nullptr;
  bool active = false;
  uint8_t info_raw[SPIREG_AUDIO_INFO_SIZE];
  char mime_raw[SPIREG_MIME_SIZE];
  volatile bool pending_info = false;
  volatile bool pending_mime = false;
  volatile uint32_t overflow_bytes = 0;
  char mime_str[SPIREG_MIME_SIZE] = {0};
  void (*mime_cb)(const char*, void*) = nullptr;
  void* mime_ctx = nullptr;

  static void encodeInfo(const AudioInfo& in, uint8_t* out) {
    out[0] = in.sample_rate & 0xFF;
    out[1] = (in.sample_rate >> 8) & 0xFF;
    out[2] = (in.sample_rate >> 16) & 0xFF;
    out[3] = (in.sample_rate >> 24) & 0xFF;
    out[4] = in.channels & 0xFF;
    out[5] = (in.channels >> 8) & 0xFF;
    out[6] = in.bits_per_sample;
    out[7] = 0;
  }

  static void decodeInfo(const uint8_t* in, AudioInfo& out) {
    out.sample_rate = (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
                      ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
    out.channels = (uint16_t)in[4] | ((uint16_t)in[5] << 8);
    out.bits_per_sample = in[6];
  }

  static void frameThunk(size_t len, void* self) {
    static_cast<SPIRegAudioSlave*>(self)->onFrame(len);
  }

  /// Processes one frame: runs in ISR / SPI task context!
  void onFrame(size_t len) {
    int c = spi.read();
    if (c < 0) return;
    len--;
    uint8_t cmd = (uint8_t)c;
    uint8_t addr = cmd & SPIREG_ADDR_MASK;
    if (cmd & SPIREG_READ_MASK) {
      size_t want = 0;
      if (len >= 1) {
        want = (uint8_t)spi.read();
        len--;
      }
      while (len--) spi.read();
      stageReply(addr, want);
    } else {
      storeWrite(addr, len);
    }
  }

  void storeWrite(uint8_t addr, size_t len) {
    switch (addr) {
      case SPIREG_DATA: {
        uint8_t chunk[32];
        while (len) {
          size_t n = len < sizeof(chunk) ? len : sizeof(chunk);
          n = spi.read(chunk, n);
          if (n == 0) break;
          len -= n;
          int free = p_buffer->availableForWrite();
          int w = free > 0 ? p_buffer->writeArray(chunk, min((int)n, free)) : 0;
          overflow_bytes = overflow_bytes + (n - w);
        }
        break;
      }
      case SPIREG_AUDIO_INFO:
        if (len == SPIREG_AUDIO_INFO_SIZE) {
          tinyspi::detail::Critical c;
          spi.read(info_raw, SPIREG_AUDIO_INFO_SIZE);
          pending_info = true;
          len = 0;
        }
        break;
      case SPIREG_MIME:
        if (len > 0 && len <= SPIREG_MIME_SIZE) {
          tinyspi::detail::Critical c;
          memset(mime_raw, 0, sizeof(mime_raw));
          spi.read((uint8_t*)mime_raw, len);
          mime_raw[SPIREG_MIME_SIZE - 1] = 0;
          pending_mime = true;
          len = 0;
        }
        break;
      default:
        break;
    }
    while (len--) spi.read();  // discard what was not consumed
  }

  void stageReply(uint8_t addr, size_t want) {
    uint8_t reg[SPIREG_MIME_SIZE];
    size_t size = 0;
    memset(reg, 0, sizeof(reg));
    switch (addr) {
      case SPIREG_AUDIO_INFO:
        {
          tinyspi::detail::Critical c;
          memcpy(reg, info_raw, SPIREG_AUDIO_INFO_SIZE);
        }
        size = SPIREG_AUDIO_INFO_SIZE;
        break;
      case SPIREG_MIME:
        {
          tinyspi::detail::Critical c;
          memcpy(reg, mime_raw, SPIREG_MIME_SIZE);
        }
        size = SPIREG_MIME_SIZE;
        break;
      case SPIREG_AVAILABLE: {
        uint32_t space = p_buffer->availableForWrite();
        reg[0] = space & 0xFF;
        reg[1] = (space >> 8) & 0xFF;
        reg[2] = (space >> 16) & 0xFF;
        reg[3] = (space >> 24) & 0xFF;
        size = SPIREG_AVAILABLE_SIZE;
        break;
      }
      default:
        break;
    }
    spi.flushTx();
    if (want == 0) want = size;
    size_t room = (size_t)spi.availableForWrite();
    if (want > room) want = room;
    for (size_t j = 0; j < want; j++) {
      spi.write(j < size ? reg[j] : 0);
    }
  }
};

}  // namespace audio_tools
