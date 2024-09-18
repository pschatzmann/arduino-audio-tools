#pragma once

#include "AudioI2S/I2SConfig.h"
#if defined(RP2040_MBED)
#include "RP2040-I2S.h"

#define IS_I2S_IMPLEMENTED 

namespace audio_tools {

//#if !defined(ARDUINO_ARCH_MBED_RP2040)
//static ::I2S I2S;
//#endif

/**
 * @brief Basic I2S API - for the ...
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SDriverRP2040MBED {
  friend class I2SStream;

 public:
  /// Provides the default configuration
  I2SConfigStd defaultConfig(RxTxMode mode) {
    I2SConfigStd c(mode);
    return c;
  }

  /// Potentially updates the sample rate (if supported)
  bool setAudioInfo(AudioInfo) { return false; }

  /// starts the DAC with the default config in TX Mode
  bool begin(RxTxMode mode = TX_MODE) {
    TRACED();
    return begin(defaultConfig(mode));
  }

  /// starts the DAC
  bool begin(I2SConfigStd cfg) {
    TRACEI();
    this->cfg = cfg;
    cfg.logInfo();
    if (cfg.rx_tx_mode != TX_MODE) {
      LOGE("Unsupported mode: only TX_MODE is supported");
      return false;
    }
    if (!I2S.setBCLK(cfg.pin_bck)) {
      LOGE("Could not set bck pin: %d", cfg.pin_bck);
      return false;
    }
    if (!I2S.setDATA(cfg.pin_data)) {
      LOGE("Could not set data pin: %d", cfg.pin_data);
      return false;
    }
    if (cfg.bits_per_sample != 16) {
      LOGE("Unsupported bits_per_sample: %d", (int)cfg.bits_per_sample);
      return false;
    }

    if (cfg.channels < 1 || cfg.channels > 2) {
      LOGE("Unsupported channels: '%d' - only 2 is supported", (int)cfg.channels);
      return false;
    }

    int rate = cfg.sample_rate;
    if (cfg.channels == 1) {
      rate = rate / 2;
    }

    if (!I2S.begin(rate)) {
      LOGE("Could not start I2S");
      return false;
    }
    return true;
  }

  /// stops the I2C and unistalls the driver
  void end() { I2S.end(); }

  /// provides the actual configuration
  I2SConfigStd config() { return cfg; }

  /// writes the data to the I2S interface
  size_t writeBytes(const void *src, size_t size_bytes) {
    TRACED();
    size_t result = 0;
    int16_t *p16 = (int16_t *)src;

    if (cfg.channels == 1) {
      int samples = size_bytes / 2;
      // multiply 1 channel into 2
      int16_t buffer[samples * 2];  // from 1 byte to 2 bytes
      for (int j = 0; j < samples; j++) {
        buffer[j * 2] = p16[j];
        buffer[j * 2 + 1] = p16[j];
      }
      result = I2S.write((const uint8_t *)buffer, size_bytes * 2) * 2;
    } else if (cfg.channels == 2) {
      result = I2S.write((const uint8_t *)src, size_bytes) * 4;
    }
    return result;
  }

  size_t readBytes(void *dest, size_t size_bytes) {
    TRACEE();
    size_t result = 0;
    return result;
  }

  int availableForWrite() {
    int result = 0;
    if (cfg.channels == 1) {
      // it should be a multiple of 2
      result = I2S.availableForWrite() / 2 * 2;
      // return half of it because we double when writing
      result = result / 2;
    } else {
      // it should be a multiple of 4
      result = I2S.availableForWrite() / 4 * 4;
    }
    if (result < 4) {
      result = 0;
    }
    return result;
  }

  int available() { return 0; }

  void flush() { return I2S.flush(); }

 protected:
  I2SConfigStd cfg;

  // blocking write
  void writeSample(int16_t sample) {
    int written = I2S.write(sample);
    while (!written) {
      delay(5);
      LOGW("written: %d ", written);
      written = I2S.write(sample);
    }
  }

  int writeSamples(int samples, int16_t *values) {
    int result = 0;
    for (int j = 0; j < samples; j++) {
      int16_t sample = values[j];
      writeSample(sample);
      writeSample(sample);
      result++;
    }
    return result;
  }
};

using I2SDriver = I2SDriverRP2040MBED;

}  // namespace audio_tools

#endif
