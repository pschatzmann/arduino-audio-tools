

#pragma once
#include "AudioConfig.h"

#if defined(USE_I2S)

#include "AudioTools/CoreAudio/AudioI2S/I2SConfig.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SESP32.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SESP32V1.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SESP8266.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SNanoSenseBLE.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SRP2040-MBED.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SRP2040.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SSAMD.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SSTM32.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioTypes.h"

#if defined(IS_I2S_IMPLEMENTED)

namespace audio_tools {

/**
 * @brief We support the Stream interface for the I2S access. In addition we
 * allow a separate mute pin which might also be used to drive a LED...
 * @ingroup io
 * @tparam T
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class I2SStream : public AudioStream {
 public:
  I2SStream() = default;

#ifdef ARDUINO
  I2SStream(int mute_pin) {
    TRACED();
    this->mute_pin = mute_pin;
    if (mute_pin > 0) {
      pinMode(mute_pin, OUTPUT);
      mute(true);
    }
  }
#endif

  /// Provides the default configuration
  I2SConfig defaultConfig(RxTxMode mode = TX_MODE) {
    return i2s.defaultConfig(mode);
  }

  bool begin() {
    TRACEI();
    I2SConfig cfg = i2s.config();
    if (!cfg){
      LOGE("unsuported AudioInfo: sample_rate: %d / channels: %d / bits_per_sample: %d", (int) cfg.sample_rate, (int)cfg.channels, (int)cfg.bits_per_sample);
      return false;
    }
    cfg.copyFrom(audioInfo());
    if (cfg.rx_tx_mode == UNDEFINED_MODE){
      cfg.rx_tx_mode = RXTX_MODE;
    }
    is_active = i2s.begin(cfg);
    mute(false);
    return is_active;
  }

  /// Starts the I2S interface
  bool begin(I2SConfig cfg) {
    TRACED();
    if (!cfg){
      LOGE("unsuported AudioInfo: sample_rate: %d / channels: %d / bits_per_sample: %d", (int) cfg.sample_rate, (int)cfg.channels, (int)cfg.bits_per_sample);
      return false;
    }
    AudioStream::setAudioInfo(cfg);
    is_active = i2s.begin(cfg);
    // unmute
    mute(false);
    return is_active;
  }

  /// Stops the I2S interface
  void end() {
    TRACEI();
    is_active = false;
    mute(true);
    i2s.end();
  }

  /// updates the sample rate dynamically
  virtual void setAudioInfo(AudioInfo info) {
    TRACEI();
    assert(info.sample_rate != 0);
    assert(info.channels != 0);
    assert(info.bits_per_sample != 0);
    AudioStream::setAudioInfo(info);
    if (is_active) {
      if (!i2s.setAudioInfo(info)) {
        I2SConfig current_cfg = i2s.config();
        if (!info.equals(current_cfg)) {
          LOGI("restarting i2s");
          info.logInfo("I2SStream");
          i2s.end();
          current_cfg.copyFrom(info);
          i2s.begin(current_cfg);
        } else {
          LOGI("no change");
        }
      }
    }
  }

  /// Writes the audio data to I2S
  virtual size_t write(const uint8_t *data, size_t len) {
    LOGD("I2SStream::write: %d", len);
    if (data == nullptr || len == 0 || !is_active)  return 0;
    return i2s.writeBytes(data, len);
  }

  /// Reads the audio data
  virtual size_t readBytes(uint8_t *data, size_t len) override {
    return i2s.readBytes(data, len);
  }

  /// Provides the available audio data
  virtual int available() override { return i2s.available(); }

  /// Provides the available audio data
  virtual int availableForWrite() override { return i2s.availableForWrite(); }

  void flush() override {}

  /// Provides access to the driver
  I2SDriver *driver() { return &i2s; }

  /// Returns true if i2s is active
  operator bool() override { return is_active; }

  /// Returns true if i2s is active
  bool isActive()  { return is_active;}

 protected:
  I2SDriver i2s;
  int mute_pin;
  bool is_active = false;

  /// set mute pin on or off
  void mute(bool is_mute) {
#ifdef ARDUINO
    if (mute_pin > 0) {
      digitalWrite(mute_pin, is_mute ? SOFT_MUTE_VALUE : !SOFT_MUTE_VALUE);
    }
#endif
  }
};

}  // namespace audio_tools

#endif
#endif
