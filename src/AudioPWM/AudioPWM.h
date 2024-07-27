#pragma once
#include "AudioConfig.h"
#if defined(USE_PWM)

#include "AudioPWM/PWMAudioESP32.h"
#include "AudioPWM/PWMAudioMBED.h"
#include "AudioPWM/PWMAudioRP2040.h"
#include "AudioPWM/PWMAudioRenesas.h"
#include "AudioPWM/PWMAudioSTM32.h"
// this is experimental at the moment
#include "AudioPWM/PWMAudioAVR.h"

namespace audio_tools {

/**
 * @brief Common functionality for PWM output.
 * Please use the PWMAudioOutput typedef instead which references the
 * implementation
 * @ingroup io
 */
class PWMAudioOutput : public AudioOutput {
 public:
  ~PWMAudioOutput() {
    if (pwm.isTimerStarted()) {
      end();
    }
  }

  virtual PWMConfig defaultConfig(RxTxMode mode=TX_MODE) { 
    if (mode!=TX_MODE) LOGE("mode not supported: using TX_MODE");
    return pwm.defaultConfig(); 
  }

  PWMConfig config() { return audio_config; }

  /// updates the sample rate dynamically
  virtual void setAudioInfo(AudioInfo info) {
    TRACEI();
    PWMConfig cfg = audio_config;
    if (cfg.sample_rate != info.sample_rate || cfg.channels != info.channels ||
        cfg.bits_per_sample != info.bits_per_sample) {
      cfg.sample_rate = info.sample_rate;
      cfg.bits_per_sample = info.bits_per_sample;
      cfg.channels = info.channels;
      cfg.logInfo();
      end();
      begin(cfg);
    }
  }

  /// starts the processing using Streams
  bool begin(PWMConfig config) {
    TRACED();
    this->audio_config = config;
    AudioOutput::setAudioInfo(audio_config);
    return pwm.begin(audio_config);
  }

  bool begin() {
    TRACED();
    AudioOutput::setAudioInfo(audio_config);
    return pwm.begin(audio_config);
  }

  virtual void end() override { pwm.end(); }

  int availableForWrite() override { return pwm.availableForWrite(); }

  // blocking write for an array: we expect a singed value and convert it into a
  // unsigned
  size_t write(const uint8_t *data, size_t len) override {
    return pwm.write(data, len);
  }

  // When the timer does not have enough data we increase the underflow_count;
  uint32_t underflowsPerSecond() { return pwm.underflowsPerSecond(); }
  // provides the effectivly measured output frames per second
  uint32_t framesPerSecond() { return pwm.framesPerSecond(); }

  /// Provides access to the driver
  PWMDriver *driver() { return &pwm; }
  /// You can assign your own custom buffer impelementation: must be allocated
  /// on the heap and will be cleaned up by this class
  void setBuffer(BaseBuffer<uint8_t> *buffer) { pwm.setBuffer(buffer); }

 protected:
  PWMConfig audio_config;
  PWMDriver pwm;  // platform specific pwm
};

// legacy name
#if USE_OBSOLETE
using PWMAudioStream = PWMAudioOutput;
#endif

}  // namespace audio_tools

#endif
