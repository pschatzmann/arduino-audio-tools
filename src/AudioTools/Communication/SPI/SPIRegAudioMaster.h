#pragma once

#include <SPI.h>

#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "SPIRegAudioRegisters.h"

namespace audio_tools {

/**
 * @brief Configuration for SPIRegAudioMaster
 * @ingroup spi-audio
 */
struct SPIRegAudioMasterConfig : public AudioInfo {
#if defined(SS)
  int cs_pin = SS;
#else
  int cs_pin = 10;
#endif
  uint32_t clock_hz = 4000000;
  decltype(MSBFIRST) bit_order = MSBFIRST;
  uint8_t data_mode = SPI_MODE0;
  /// Max audio bytes per frame: must not be bigger than the slave's
  /// max_payload
  size_t max_payload = 256;
  /// Pause (us) between a read request and the reply frame
  uint32_t reply_gap_us = 300;
  /// true: write() waits until the slave has enough space
  bool is_blocking_write = false;
};

/**
 * @brief SPI master which streams audio to a SPIRegAudioSlave (based on the
 * TinySPISlave library) using its register map: it defines the AudioInfo and
 * the mime type and writes the audio data according to the space which is
 * reported by the available-space register.
 * @ingroup spi-audio
 * @code
 * SPIRegAudioMaster master;
 * auto cfg = SPIRegAudioMasterConfig();
 * cfg.sample_rate = 44100; cfg.channels = 2; cfg.bits_per_sample = 16;
 * master.begin(cfg);
 * master.setMime("audio/pcm");
 * master.write(data, len);
 * @endcode
 */
class SPIRegAudioMaster : public AudioOutput {
 public:
  SPIRegAudioMaster() = default;
  explicit SPIRegAudioMaster(SPIClass& spi) : p_spi(&spi) {}

  bool begin(SPIRegAudioMasterConfig cfg = SPIRegAudioMasterConfig()) {
    config = cfg;
    if (config.max_payload == 0) return false;
    if (p_spi == nullptr) p_spi = &SPI;
    if (config.cs_pin >= 0) {
      pinMode(config.cs_pin, OUTPUT);
      digitalWrite(config.cs_pin, HIGH);
    }
    p_spi->begin();
    tx.resize(max((size_t)SPIREG_MIME_SIZE, config.max_payload) + 2);
    active = true;
    if (cfg) setAudioInfo(cfg);
    return true;
  }

  void end() {
    if (p_spi != nullptr && active) p_spi->end();
    active = false;
  }

  bool isActive() const { return active; }

  /// Defines the mime type (max 31 characters) on the slave
  bool setMime(const char* mime) {
    if (mime == nullptr) return false;
    uint8_t buf[SPIREG_MIME_SIZE] = {0};
    strncpy((char*)buf, mime, SPIREG_MIME_SIZE - 1);
    return writeRegister(SPIREG_MIME, buf, sizeof(buf));
  }

  /// Defines the audio format on the slave
  void setAudioInfo(AudioInfo info) override {
    AudioOutput::setAudioInfo(info);
    uint8_t buf[SPIREG_AUDIO_INFO_SIZE];
    buf[0] = info.sample_rate & 0xFF;
    buf[1] = (info.sample_rate >> 8) & 0xFF;
    buf[2] = (info.sample_rate >> 16) & 0xFF;
    buf[3] = (info.sample_rate >> 24) & 0xFF;
    buf[4] = info.channels & 0xFF;
    buf[5] = (info.channels >> 8) & 0xFF;
    buf[6] = info.bits_per_sample;
    buf[7] = 0;
    if (active) writeRegister(SPIREG_AUDIO_INFO, buf, sizeof(buf));
  }

  /// Reads the audio format which is currently defined on the slave
  AudioInfo readAudioInfo() {
    uint8_t buf[SPIREG_AUDIO_INFO_SIZE] = {0};
    AudioInfo result;
    if (readRegister(SPIREG_AUDIO_INFO, buf, sizeof(buf))) {
      result.sample_rate = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
      result.channels = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
      result.bits_per_sample = buf[6];
    }
    return result;
  }

  /// Free space in the slave's audio buffer in bytes
  int availableForWrite() override {
    if (!active) return 0;
    uint8_t buf[SPIREG_AVAILABLE_SIZE] = {0};
    if (!readRegister(SPIREG_AVAILABLE, buf, sizeof(buf))) return 0;
    return (int)((uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                 ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24));
  }

  /// Writes the audio data; returns the number of bytes which were accepted
  /// (all of them in blocking mode)
  size_t write(const uint8_t* data, size_t len) override {
    if (!active || data == nullptr) return 0;
    size_t written = 0;
    while (written < len) {
      size_t chunk = min(len - written, config.max_payload);
      int space = availableForWrite();
      if (space < (int)chunk) {
        if (!config.is_blocking_write) {
          chunk = space;  // send what fits
        } else {
          delay(5);
          continue;
        }
      }
      if (chunk == 0) break;
      if (!writeRegister(SPIREG_DATA, data + written, chunk)) break;
      written += chunk;
    }
    return written;
  }

 protected:
  SPIRegAudioMasterConfig config;
  SPIClass* p_spi = nullptr;
  bool active = false;
  Vector<uint8_t> tx{0};

  /// Write frame: [addr][bytes...]
  bool writeRegister(uint8_t addr, const uint8_t* data, size_t len) {
    if (len + 1 > tx.size()) return false;
    tx[0] = addr & SPIREG_ADDR_MASK;
    memcpy(tx.data() + 1, data, len);
    select();
    p_spi->transfer(tx.data(), len + 1);
    deselect();
    return true;
  }

  /// Read request frame, pause, reply frame
  bool readRegister(uint8_t addr, uint8_t* out, size_t len) {
    if (!active || len == 0 || len + 1 > tx.size()) return false;
    uint8_t req[2] = {(uint8_t)(SPIREG_READ_MASK | (addr & SPIREG_ADDR_MASK)),
                      (uint8_t)len};
    select();
    p_spi->transfer(req, sizeof(req));
    deselect();
    delayMicroseconds(config.reply_gap_us);
    memset(out, 0, len);
    select();
    p_spi->transfer(out, len);  // dummy 0x00 bytes: replaced by the reply
    deselect();
    return true;
  }

  void select() {
    p_spi->beginTransaction(
        SPISettings(config.clock_hz, config.bit_order, config.data_mode));
    if (config.cs_pin >= 0) digitalWrite(config.cs_pin, LOW);
  }

  void deselect() {
    if (config.cs_pin >= 0) digitalWrite(config.cs_pin, HIGH);
    p_spi->endTransaction();
  }
};

}  // namespace audio_tools
