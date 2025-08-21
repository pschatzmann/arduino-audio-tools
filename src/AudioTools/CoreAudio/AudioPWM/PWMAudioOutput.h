#pragma once
#include "AudioToolsConfig.h"
#if defined(USE_PWM)

#include "AudioTools/CoreAudio/AudioPWM/PWMDriverESP32.h"
#include "AudioTools/CoreAudio/AudioPWM/PWMDriverMBED.h"
#include "AudioTools/CoreAudio/AudioPWM/PWMDriverRP2040.h"
#include "AudioTools/CoreAudio/AudioPWM/PWMDriverRenesas.h"
#include "AudioTools/CoreAudio/AudioPWM/PWMDriverSTM32.h"
// this is experimental at the moment
#include "AudioTools/CoreAudio/AudioPWM/PWMDriverAVR.h"

namespace audio_tools {

/**
 * @brief Common functionality for PWM output. We generate audio using PWM
 * with a frequency that is above the hearing range. The sample rate is 
 * usually quite restricted, so we also automatically decimate the data.
 * Further info see PWMConfig
 * @ingroup io
 */
class PWMAudioOutput : public AudioOutput {
 public:
  // Default constructor with default driver
  PWMAudioOutput() = default;

  // Inject external driver (must live longer than this object)
  PWMAudioOutput(DriverPWMBase &ext_driver)
      : p_driver(&ext_driver) {}

  ~PWMAudioOutput() {
  if (p_driver && p_driver->isTimerStarted()) {
      end();
    }
  }

  virtual PWMConfig defaultConfig(RxTxMode mode=TX_MODE) { 
    if (mode!=TX_MODE) LOGE("mode not supported: using TX_MODE");
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

  virtual void end() override { if (p_driver) p_driver->end(); }

  int availableForWrite() override { return p_driver ? p_driver->availableForWrite() : 0; }

  size_t write(const uint8_t *data, size_t len) override {
    return p_driver ? p_driver->write(data, len) : 0; }

  uint32_t underflowsPerSecond() { return p_driver ? p_driver->underflowsPerSecond() : 0; }
  uint32_t framesPerSecond() { return p_driver ? p_driver->framesPerSecond() : 0; }

  DriverPWMBase *driver() { return p_driver; }
  void setBuffer(BaseBuffer<uint8_t> *buffer) { if (p_driver) p_driver->setBuffer(buffer); }

 protected:
  PWMConfig audio_config;
  PWMDriver default_driver;
  DriverPWMBase *p_driver = &default_driver;
};


}  // namespace audio_tools

#endif
