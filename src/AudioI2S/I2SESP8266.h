#pragma once

#ifdef ESP8266
#include <I2S.h>

#include "AudioI2S/I2SConfig.h"
#include "AudioTools/AudioLogger.h"

#define IS_I2S_IMPLEMENTED 

namespace audio_tools {

/**
 * @brief Basic I2S API - for the ESP8266
 * Only 16 bits are supported !
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SDriverESP8266 {
  friend class I2SStream;

 public:
  /// Provides the default configuration
  I2SConfigStd defaultConfig(RxTxMode mode) {
    I2SConfigStd c(mode);
    return c;
  }

  /// Potentially updates the sample rate (if supported)
  bool setAudioInfo(AudioInfo) { return false; }

  /// starts the DAC with the default config
  bool begin(RxTxMode mode = TX_MODE) { return begin(defaultConfig(mode)); }

  /// starts the DAC
  bool begin(I2SConfigStd cfg) {
    this->cfg = cfg;
    i2s_set_rate(cfg.sample_rate);
    cfg.bits_per_sample = 16;
    if (!i2s_rxtx_begin(cfg.rx_tx_mode == RX_MODE, cfg.rx_tx_mode == TX_MODE)) {
      LOGE("i2s_rxtx_begin failed");
      return false;
    }
    return true;
  }

  /// stops the I2C and unistalls the driver
  void end() { i2s_end(); }

  /// we assume the data is already available in the buffer
  int available() { return I2S_BUFFER_COUNT * I2S_BUFFER_SIZE; }

  /// We limit the write size to the buffer size
  int availableForWrite() { return I2S_BUFFER_COUNT * I2S_BUFFER_SIZE; }

  /// provides the actual configuration
  I2SConfigStd config() { return cfg; }

 protected:
  I2SConfigStd cfg;

  /// writes the data to the I2S interface
  size_t writeBytes(const void *src, size_t size_bytes) {
    size_t result = 0;
    size_t frame_size = cfg.channels * (cfg.bits_per_sample / 8);
    uint16_t frame_count = size_bytes / frame_size;
    if (cfg.channels == 2 && cfg.bits_per_sample == 16) {
      result = i2s_write_buffer((int16_t *)src, frame_count) * frame_size;
    } else {
      result = writeExt(src, size_bytes);
    }
    return result;
  }

  /// writes the data by making shure that we send 2 channels 16 bit data
  size_t writeExt(const void *src, size_t size_bytes) {
    size_t result = 0;
    int j;

    switch (cfg.bits_per_sample) {
      case 8:
        for (j = 0; j < size_bytes; j += cfg.channels) {
          int8_t *data = (int8_t *)src;
          int16_t frame[2];
          frame[0] = data[j] * 256;
          if (cfg.channels == 1) {
            frame[1] = data[j] * 256;
          } else {
            frame[1] = data[j + 1] * 256;
          }
          uint32_t *frame_ptr = (uint32_t *)frame;
          if (i2s_write_sample(*frame_ptr)) {
            result += 2;
          }
        }
        break;

      case 16:
        for (j = 0; j < size_bytes / 2; j += cfg.channels) {
          int16_t *data = (int16_t *)src;
          int16_t frame[2];
          frame[0] = data[j];
          if (cfg.channels == 1) {
            frame[1] = data[j];
          } else {
            frame[1] = data[j + 1];
          }
          uint32_t *frame_ptr = (uint32_t *)frame;
          if (i2s_write_sample(*frame_ptr)) {
            result += 2;
          }
        }
        break;

      case 24:
        for (j = 0; j < size_bytes / 3; j += cfg.channels) {
          int24_t *data = (int24_t *)src;
          int24_t frame[2];
          int32_t value = data[j];
          frame[0] = value;
          if (cfg.channels == 1) {
            frame[1] = value;
          } else {
            value = data[j + 1];
            frame[1] = value / 256;
          }
          uint32_t *frame_ptr = (uint32_t *)frame;
          if (i2s_write_sample(*frame_ptr)) {
            result += 2;
          }
        }
        break;

      case 32:
        for (j = 0; j < size_bytes / 4; j += cfg.channels) {
          int32_t *data = (int32_t *)src;
          int32_t frame[2];
          frame[0] = data[j] / 65538;
          if (cfg.channels == 1) {
            frame[1] = data[j] / 65538;
          } else {
            frame[1] = data[j + 1] / 65538;
          }
          uint32_t *frame_ptr = (uint32_t *)frame;
          if (i2s_write_sample(*frame_ptr)) {
            result += 2;
          }
        }
        break;
    }
    return result;
  }

  /// reads the data from the I2S interface
  size_t readBytes(void *dest, size_t size_bytes) {
    size_t result_bytes = 0;
    uint16_t frame_size = cfg.channels * (cfg.bits_per_sample / 8);
    size_t frames = size_bytes / frame_size;
    int16_t *ptr = (int16_t *)dest;

    for (int j = 0; j < frames; j++) {
      int16_t *left = ptr;
      int16_t *right = ptr + 1;
      bool ok = i2s_read_sample(
          left, right, false);  // RX data returned in both 16-bit outputs.
      if (!ok) {
        break;
      }
      ptr += 2;
      result_bytes += frame_size;
    }
    return result_bytes;
  }
};

using I2SDriver = I2SDriverESP8266;

}  // namespace audio_tools

#endif
