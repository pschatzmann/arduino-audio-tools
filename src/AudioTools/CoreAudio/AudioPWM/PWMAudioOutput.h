#pragma once
#include "AudioToolsConfig.h"
#if defined(USE_PWM)

#include "AudioTools/CoreAudio/AudioPWM/PWMDriverESP32.h"
#include "AudioTools/CoreAudio/AudioPWM/PWMDriverMBED.h"
#include "AudioTools/CoreAudio/AudioPWM/PWMDriverRP2040.h"
#include "AudioTools/CoreAudio/AudioPWM/PWMDriverRenesas.h"
#include "AudioTools/CoreAudio/AudioPWM/PWMDriverSTM32.h"
#include "AudioTools/CoreAudio/AudioPWM/PWMDriverZephyr.h"
// this is experimental at the moment
#include "AudioTools/CoreAudio/AudioPWM/PWMDriverAVR.h"
#include "PWMConfig.h"

namespace audio_tools {

/**
 * @brief Common functionality for PWM output. We generate audio using PWM
 * with a frequency that is above the hearing range. The sample rate is
 * usually quite restricted, so we also automatically decimate the data.
 * Further info see PWMConfig
 * @ingroup io
 */

template<class PWMDriverT = PWMDriver>
class PWMAudioOutput : public AudioOutput {
 public:
  // Default constructor with default driver
  PWMAudioOutput() = default;
  PWMAudioOutput(PWMDriverBase &driver) { p_driver = &driver; }
  ~PWMAudioOutput() {
    if (p_driver->isTimerStarted()) {
      end();
    }
  }

  virtual PWMConfig defaultConfig(RxTxMode mode = TX_MODE) {
    if (mode != TX_MODE) LOGE("mode not supported: using TX_MODE");
    return p_driver->defaultConfig();
  }

  PWMConfig config() { return audio_config; }

  /// updates the sample rate dynamically
  void setAudioInfo(AudioInfo info) override {
    TRACEI();
    AudioOutput::cfg = info;
    PWMConfig cfg = audio_config;
    if (cfg.sample_rate != info.sample_rate || cfg.channels != info.channels ||
        cfg.bits_per_sample != info.bits_per_sample) {
      cfg.sample_rate = info.sample_rate;
      cfg.bits_per_sample = info.bits_per_sample;
      cfg.channels = info.channels;
      end();
      begin(cfg);
      cfg.logInfo();
    }
  }

  AudioInfo audioInfoOut() override {
    AudioInfo result = audioInfo();
    result.sample_rate = p_driver->effectiveOutputSampleRate();
    return result;
  }

  /// starts the processing using Streams
  bool begin(PWMConfig config) {
    TRACED();
    this->audio_config = config;
    return begin();
  }

  bool begin() {
    TRACED();
    AudioOutput::setAudioInfo(audio_config);
    return p_driver->begin(audio_config);
  }

  virtual void end() override { p_driver->end(); }

  int availableForWrite() override { return p_driver->availableForWrite(); }

  size_t write(const uint8_t* data, size_t len) override {
    return p_driver->write(data, len);
  }

  uint32_t underflowsPerSecond() { return p_driver->underflowsPerSecond(); }
  uint32_t framesPerSecond() { return p_driver->framesPerSecond(); }

  PWMDriverBase* driver() { return p_driver; }
  void setBuffer(BaseBuffer<uint8_t>* buffer) { p_driver->setBuffer(buffer); }

 protected:
  PWMConfig audio_config;
  PWMDriver pwm_driver;
  PWMDriverBase* p_driver = &pwm_driver;
};

}  // namespace audio_tools

#endif
