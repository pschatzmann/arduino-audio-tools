#pragma once
#if defined(ARDUINO_ARCH_TANGNANO20K)
#include "AudioTools/CoreAudio/AudioPWM/PWMDriverBase.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimer.h"
#if TANGNANO20K_PWM_AUDIO
#include <PWMAudio.h>
#endif

namespace audio_tools {

/**
 * @brief Audio output to PWM pins for the Tang Nano 20K using analogWrite():
 * the samples are written by a timer callback. Up to 6 pins can be used
 * (GPIO0 - GPIO20 or the LEDs). Because the PicoRV32 is quite slow, the
 * sample rate is limited (see PWM_MAX_SAMPLE_RATE) and higher sample rates
 * are decimated.
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class PWMDriverTangNanoTimer : public PWMDriverBase {
 public:
  PWMDriverTangNanoTimer() { LOGD("PWMDriverTangNanoTimer"); }

  // Ends the output
  virtual void end() override {
    TRACED();
    ticker.end();
    is_timer_started = false;
    for (int j = 0; j < pins.size(); j++) {
      pinMode(pins[j], OUTPUT);  // releases the PWM channel
    }
    pins.clear();
    deleteBuffer();
  }

 protected:
  Vector<int> pins;
  AudioTimer ticker;  // calls a callback repeatedly with a timeout

  /// when we get the first write -> we activate the timer to start with the
  /// output of data
  virtual void startTimer() override {
    TRACED();
    if (!is_timer_started) {
      ticker.setCallbackParameter(this);
      int sample_rate = effectiveOutputSampleRate();
      if (isDecimateActive()) {
        LOGI("Using reduced sample rate: %d", sample_rate);
      }
      is_timer_started =
          ticker.begin(defaultPWMAudioOutputCallback, sample_rate, HZ);
    }
  }

  /// Setup PWM Pins
  virtual void setupPWM() {
    TRACED();
    if (audio_config.pwm_frequency == 0) {
      audio_config.pwm_frequency = PWM_AUDIO_FREQUENCY;
    }
    analogWriteResolution(audio_config.resolution);

    pins.resize(audio_config.channels);
    for (int j = 0; j < audio_config.channels; j++) {
      int gpio = audio_config.pins()[j];
      LOGI("PWM Pin: %d", gpio);
      pins[j] = gpio;
      analogWriteFrequency(gpio, audio_config.pwm_frequency);
      analogWrite(gpio, maxOutputValue() / 2);
    }
  }

  /// not used -> see startTimer();
  virtual void setupTimer() {}

  virtual int maxChannels() { return PIN_PWM_COUNT; };

  /// provides the max value for the configured resulution
  virtual int maxOutputValue() { return (1 << audio_config.resolution) - 1; }

  /// write a pwm value to the indicated channel. The max value depends on the
  /// resolution
  virtual void pwmWrite(int channel, int value) {
    analogWrite(pins[channel], value);
  }

  /// timer callback: write the next frame to the pins
  static void defaultPWMAudioOutputCallback(void *obj) {
    PWMDriverTangNanoTimer *self = (PWMDriverTangNanoTimer *)obj;
    if (self != nullptr) {
      self->playNextFrame();
    }
  }
};

#if TANGNANO20K_PWM_AUDIO

/**
 * @brief Audio output to PWM pins for the Tang Nano 20K using the PWMAudio
 * library of the core: the samples are paced and scaled by the gateware, so
 * there is no timer and no sample rate limitation. Requires Tools > PWM
 * Audio: Enabled. The pins are fixed: GPIO16 (left) and GPIO17 (right).
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class PWMDriverTangNano : public PWMDriverBase {
 public:
  PWMDriverTangNano() { LOGD("PWMDriverTangNano"); }

  /// Default configuration: pwm_frequency 0 uses PWM_AUDIO_FREQUENCY
  virtual PWMConfig defaultConfig() {
    PWMConfig cfg;
    cfg.sample_rate = 44100;
    cfg.channels = 2;
    return cfg;
  }

  virtual void end() override {
    TRACED();
    ::PWMAudio.end();
    is_timer_started = false;
    deleteBuffer();
  }

  /// the data is written directly to the PWMAudio ring buffer
  virtual size_t write(const uint8_t *data, size_t len) override {
    if (!is_timer_started) return 0;
    if (audio_config.bits_per_sample == 16) {
      return ::PWMAudio.write(data, len);
    }
    // convert to 16 bits
    int bytes = bytesPerSample();
    size_t samples = len / bytes;
    int16_t tmp[64];
    size_t pos = 0;
    while (pos < samples) {
      size_t n = min(samples - pos, (size_t)64);
      for (size_t j = 0; j < n; j++) {
        tmp[j] = toInt16(data + (pos + j) * bytes);
      }
      ::PWMAudio.write((const uint8_t *)tmp, n * sizeof(int16_t));
      pos += n;
    }
    return samples * bytes;
  }

  /// provides the free space in bytes of the configured bits_per_sample
  virtual int availableForWrite() override {
    return ::PWMAudio.availableForWrite() / 2 * bytesPerSample();
  }

  int effectiveOutputSampleRate() override { return audio_config.sample_rate; }

 protected:
  virtual void setupPWM() {
    TRACED();
    if (audio_config.pwm_frequency == 0) {
      audio_config.pwm_frequency = PWM_AUDIO_FREQUENCY;
    }
    PWMAudioConfig cfg = ::PWMAudio.defaultConfig();
    cfg.sampleRate = audio_config.sample_rate;
    cfg.channels = audio_config.channels;
    cfg.pwmRate = audio_config.pwm_frequency;
    // buffer size is in bytes: the ring buffer is in frames
    int frames = audio_config.buffer_size * audio_config.buffers /
                 (audio_config.channels * bytesPerSample());
    if (frames > 0xFFFF) frames = 0xFFFF;
    if (frames > 0) cfg.ringSamples = frames;
    is_timer_started = ::PWMAudio.begin(cfg);
    if (!is_timer_started) {
      LOGE("PWMAudio.begin failed");
    }
  }

  /// not used: the gateware paces the output
  virtual void setupTimer() {}
  virtual void startTimer() {}
  virtual int maxChannels() { return 2; };
  virtual int maxOutputValue() { return 0xFFFF; }
  virtual void pwmWrite(int channel, int value) {}
  bool isDecimateActive() override { return false; }
  int decimation() override { return 1; }

  /// 24 bit samples are stored in an int24_t
  int bytesPerSample() {
    return audio_config.bits_per_sample == 24 ? sizeof(int24_t)
                                              : audio_config.bits_per_sample / 8;
  }

  int16_t toInt16(const uint8_t *data) {
    switch (audio_config.bits_per_sample) {
      case 8:
        return (int16_t)(*(int8_t *)data) << 8;
      case 24:
        return (int32_t)(*(int24_t *)data) >> 8;
      case 32:
        return (*(int32_t *)data) >> 16;
      default:
        return *(int16_t *)data;
    }
  }
};

/// @brief Please use PWMAudioOutput!
using PWMDriver = PWMDriverTangNano;

#else

/// @brief Please use PWMAudioOutput!
using PWMDriver = PWMDriverTangNanoTimer;

#endif

}  // namespace audio_tools

#endif
