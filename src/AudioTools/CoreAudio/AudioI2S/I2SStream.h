

#pragma once
#include "AudioToolsConfig.h"

#if defined(USE_I2S)

#include "AudioTools/CoreAudio/AudioI2S/I2SConfig.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SDriverESP32.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SDriverESP32V1.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SDriverESP8266.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SDriverZephyr.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SDriverZephyrSAI.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SDriverNanoSenseBLE.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SDriverRP2040-MBED.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SDriverRP2040.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SDriverSAMD.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SDriverSTM32.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioTypes.h"

#if defined(IS_I2S_IMPLEMENTED)

namespace audio_tools {

/**
 * @brief We support the Stream interface for the I2S access. In addition we
 * allow a separate mute pin which might also be used to drive a LED...
 * @ingroup io
 * @tparam I2SDriverT with I2SDriver as default
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @notes In Zephyr we can alternatively use the SAI driver:
 * I2SStream<SAIDriver> sai
 */

class I2SStream : public AudioStream {
 public:
  I2SStream() = default;
  I2SStream(I2SDriverBase &driver) { p_driver = &driver; }
  ~I2SStream() { end(); }

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
    return p_driver->defaultConfig(mode);
  }

  bool begin() {
    TRACEI();
    I2SConfig cfg = p_driver->config();
    if (!cfg){
      LOGE("unsuported AudioInfo: sample_rate: %d / channels: %d / bits_per_sample: %d", (int) cfg.sample_rate, (int)cfg.channels, (int)cfg.bits_per_sample);
      return false;
    }
    cfg.copyFrom(audioInfo());
    if (cfg.rx_tx_mode == UNDEFINED_MODE){
      cfg.rx_tx_mode = RXTX_MODE;
    }
    is_active = p_driver->begin(cfg);
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
    is_active = p_driver->begin(cfg);
    // unmute
    mute(false);
    return is_active;
  }

  /// Stops the I2S interface
  void end() {
    if (is_active) {
      TRACEI();
      is_active = false;
      mute(true);
      p_driver->end();
    }
  }

  /// updates the sample rate dynamically
  virtual void setAudioInfo(AudioInfo info) {
    TRACEI();
    assert(info.sample_rate != 0);
    assert(info.channels != 0);
    assert(info.bits_per_sample != 0);
    AudioStream::setAudioInfo(info);
    if (is_active) {
      if (!p_driver->setAudioInfo(info)) {
        I2SConfig current_cfg = p_driver->config();
        if (!info.equals(current_cfg)) {
          LOGI("restarting i2s");
          info.logInfo("I2SStream");
          p_driver->end();
          current_cfg.copyFrom(info);
          p_driver->begin(current_cfg);
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
    return p_driver->writeBytes(data, len);
  }

  /// Reads the audio data
  virtual size_t readBytes(uint8_t *data, size_t len) override {
    return p_driver->readBytes(data, len);
  }

  /// Provides the available audio data
  virtual int available() override { return p_driver->available(); }

  /// Provides the available audio data
  virtual int availableForWrite() override { return p_driver->availableForWrite(); }

  void flush() override {}

  /// Provides access to the driver
  I2SDriverBase* driver() { return p_driver; }

  /// Returns true if i2s is active
  operator bool() override { return is_active; }

  /// Returns true if i2s is active
  bool isActive()  { return is_active;}

 protected:
  I2SDriver i2s;
  I2SDriverBase* p_driver = &i2s;
  bool is_active = false;

#ifdef ARDUINO
  int mute_pin = -1;
  /// set mute pin on or off
  void mute(bool is_mute) {
    if (mute_pin > 0) {
      digitalWrite(mute_pin, is_mute ? SOFT_MUTE_VALUE : !SOFT_MUTE_VALUE);
    }
#endif
  }
};

}  // namespace audio_tools

#endif
#endif
