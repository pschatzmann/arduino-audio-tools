#pragma once

#include "AudioTools/CoreAudio/AudioI2S/I2SConfig.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SDriverBase.h"
#if defined(ARDUINO_ARCH_TANGNANO20K)
// The core defines a global I2SConfig which would be ambiguous with
// audio_tools::I2SConfig because of the automatic using namespace
#define I2SConfig TangNanoI2SConfig
#include <I2STangNano.h>
#undef I2SConfig

#define IS_I2S_IMPLEMENTED

namespace audio_tools {

/**
 * @brief Basic I2S API for the Tang Nano 20K: we use the I2S library of the
 * core which outputs to the onboard MAX98357A amplifier. Input is only
 * available if Tools > I2S Input is enabled. The pins are defined by the
 * gateware, so the pin settings in the config are ignored. The hardware
 * always uses 16 bits: other bit widths are converted by the core library.
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SDriverTangNano : public I2SDriverBase {
  friend class I2SStream;

 public:
  /// Provides the default configuration
  I2SConfigStd defaultConfig(RxTxMode mode) {
    I2SConfigStd c(mode);
    return c;
  }

  /// Potentially updates values dynamically: restart is needed
  bool setAudioInfo(AudioInfo info) { return info.equals(cfg); }

  /// starts the DAC with the default config in TX Mode
  bool begin(RxTxMode mode = TX_MODE) { return begin(defaultConfig(mode)); }

  /// starts the DAC
  bool begin(I2SConfigStd cfg) {
    TRACEI();
    if (is_active) end();
    this->cfg = cfg;
    cfg.logInfo();

    if (!cfg.is_master) {
      LOGE("Only master mode is supported");
      return false;
    }
    if (cfg.i2s_format != I2S_STD_FORMAT &&
        cfg.i2s_format != I2S_PHILIPS_FORMAT) {
      LOGE("Only the I2S (Philips) format is supported");
      return false;
    }
    if (cfg.channels < 1 || cfg.channels > 2) {
      LOGE("Unsupported channels: %d", cfg.channels);
      return false;
    }

    TangNanoI2SConfig tang_cfg;
    tang_cfg.sampleRate = cfg.sample_rate;
    tang_cfg.channels = cfg.channels;
    switch (cfg.rx_tx_mode) {
      case TX_MODE:
        tang_cfg.mode = I2S_MODE_OUTPUT;
        break;
      case RX_MODE:
        tang_cfg.mode = I2S_MODE_INPUT;
        break;
      default:
        tang_cfg.mode = I2S_MODE_DUPLEX;
        break;
    }
    switch (cfg.bits_per_sample) {
      case 8:
        tang_cfg.bits = I2S_BITS_8;
        break;
      case 16:
        tang_cfg.bits = I2S_BITS_16;
        break;
      case 24:
#ifdef USE_3BYTE_INT24
        tang_cfg.bits = I2S_BITS_24;
#else
        // int24_4bytes_t is stored as a left aligned 32 bit value
        tang_cfg.bits = I2S_BITS_32;
#endif
        break;
      case 32:
        tang_cfg.bits = I2S_BITS_32;
        break;
      default:
        LOGE("Unsupported bits_per_sample: %d", cfg.bits_per_sample);
        return false;
    }
    if (cfg.buffer_size > 0 && cfg.buffer_count > 0) {
      // buffer size is in bytes: the ring buffer is in frames
      int frame_size = cfg.channels * bytesPerSample();
      int frames = cfg.buffer_size * cfg.buffer_count / frame_size;
      if (frames > 0xFFFF) frames = 0xFFFF;
      tang_cfg.ringSamples = frames;
    }

    ::I2S.begin(tang_cfg);
    is_active = true;
    return true;
  }

  /// stops the I2S
  void end() {
    ::I2S.end();
    is_active = false;
  }

  /// provides the actual configuration
  I2SConfigStd config() { return cfg; }

  /// writes the data to the I2S interface
  size_t writeBytes(const void *src, size_t size_bytes) {
    return ::I2S.write((const uint8_t *)src, size_bytes);
  }

  size_t readBytes(void *dest, size_t size_bytes) {
    return ::I2S.readBytes((uint8_t *)dest, size_bytes);
  }

  /// the core reports the free frames: we need bytes
  int availableForWrite() {
    return ::I2S.availableForWrite() * cfg.channels * bytesPerSample();
  }

  int available() { return ::I2S.available(); }

  void flush() { ::I2S.flush(); }

 protected:
  I2SConfigStd cfg;
  bool is_active = false;

  /// 24 bit samples are stored in an int24_t
  int bytesPerSample() {
    return cfg.bits_per_sample == 24 ? sizeof(int24_t)
                                     : cfg.bits_per_sample / 8;
  }
};

using I2SDriver = I2SDriverTangNano;

}  // namespace audio_tools

#endif
