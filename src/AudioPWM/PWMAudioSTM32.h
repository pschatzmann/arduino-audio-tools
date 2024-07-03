
#pragma once
#if defined(STM32)
#include "AudioPWM/PWMAudioBase.h"
#include "AudioTimer/AudioTimer.h"

namespace audio_tools {

// forward declaration
class PWMDriverSTM32;
/**
 * @typedef  DriverPWMBase
 * @brief Please use DriverPWMBase!
 */
using PWMDriver = PWMDriverSTM32;

/**
 * @brief Audio output to PWM pins for STM32. We use one timer to generate the
 * sample rate and one timer for the PWM signal.
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class PWMDriverSTM32 : public DriverPWMBase {
  /// @brief PWM information for a single pin
  struct PWMPin {
    HardwareTimer *p_timer;
    int channel;
    int max_value;
    bool active = false;
    int pin;
    int pwm_frequency;

    PWMPin() = default;

    PWMPin(HardwareTimer *p_timer, int channel, int pin, int maxValue,
           int pwmFrequency = 30000) {
      this->p_timer = p_timer;
      this->channel = channel;
      this->pin = pin;
      this->max_value = maxValue;
      this->pwm_frequency = pwmFrequency;
    }

    void begin() {
      TRACEI();
      p_timer->setPWM(channel, pin, pwm_frequency,
                      50);  // 30k Hertz, 50% dutycycle
      active = true;
    }

    void setRate(int rate) {
      if (active) {
        uint16_t sample = 100.0 * rate / max_value;
        p_timer->setCaptureCompare(channel, sample,
                                   PERCENT_COMPARE_FORMAT);  // 50%
      }
    }
  };

  class PWM {
   public:
    PWM() = default;

    void begin(HardwareTimer *pwm_timer, int pwm_frequency, int maxValue) {
      this->p_timer = pwm_timer;
      this->pwm_frequency = pwm_frequency;
      this->max_value = maxValue;
    }

    void end() {
      p_timer->pause();
    }

    bool addPin(int pin) {
      LOGI("addPin: %d", pin);
      TIM_TypeDef *p_instance = (TIM_TypeDef *)pinmap_peripheral(
          digitalPinToPinName(pin), PinMap_PWM);
      channel = STM_PIN_CHANNEL(
          pinmap_function(digitalPinToPinName(pin), PinMap_PWM));
      PWMPin pwm_pin{p_timer, channel, pin, max_value, pwm_frequency};
      pins.push_back(pwm_pin);
      // make sure that all pins use the same timer !
      if (p_timer->getHandle()->Instance != p_instance) {
        LOGE("Invalid pin %d with timer %s for timer %s", pin,
             getTimerStr(p_instance),
             getTimerStr(p_timer->getHandle()->Instance));
        return false;
      }
      LOGI("Using Timer %s for PWM", getTimerStr(p_instance));
      pins[pins.size() - 1].begin();
      return true;
    }

    void setRate(int idx, int rate) {
      if (idx < pins.size()) {
        pins[idx].setRate(rate);
      } else {
        LOGE("Invalid index: %d", idx);
      }
    }

   protected:
    HardwareTimer *p_timer;
    audio_tools::Vector<PWMPin> pins;
    int channel;
    int max_value;
    int pwm_frequency;

    const char *getTimerStr(TIM_TypeDef *inst) {
      if (inst == TIM1)
        return "TIM1";
      else if (inst == TIM2)
        return "TIM2";
      else if (inst == TIM3)
        return "TIM3";
      else if (inst == TIM4)
        return "TIM4";
      else if (inst == TIM5)
        return "TIM5";
      return "N/A";
    }
  };

 public:
  PWMDriverSTM32() {
    TRACED();
    ticker.setTimer(PWM_FREQ_TIMER_NO);
  }

  // Ends the output
  virtual void end() override {
    TRACED();
    ticker.end();  // it does not hurt to call this even if it has not been
                   // started
    pwm.end();     // stop pwm timer
    deleteBuffer();
    is_timer_started = false;
    if (buffer != nullptr) {
      delete buffer;
      buffer = nullptr;
    }
  }

  /// Defines the timer which is used to generate the PWM signal
  void setPWMTimer(HardwareTimer &t) { p_pwm_timer = &t; }

 protected:
  TimerAlarmRepeating ticker;  // calls a callback repeatedly with a timeout
  HardwareTimer *p_pwm_timer = nullptr;
  PWM pwm;
  int64_t max_value;

  /// when we get the first write -> we activate the timer to start with the
  /// output of data
  virtual void startTimer() override {
    if (!is_timer_started) {
      TRACED();
      uint32_t time = AudioTime::toTimeUs(audio_config.sample_rate);
      ticker.setCallbackParameter(this);
      ticker.begin(defaultPWMAudioOutputCallback, time, US);
      is_timer_started = true;
    }
  }

  /// Setup PWM Pins
  virtual void setupPWM() {
    TRACED();

    // setup pwm timer
    if (p_pwm_timer == nullptr) {
      p_pwm_timer = new HardwareTimer(PWM_DEFAULT_TIMER);
    }

    // setup pins for output
    int ch = 0;
    pwm.begin(p_pwm_timer, audio_config.pwm_frequency, maxOutputValue());
    for (auto gpio : audio_config.pins()) {
      LOGD("Processing channel %d -> pin: %d", ch++, gpio);
      pwm.addPin(gpio);
    }
  }

  virtual void setupTimer() {}

  /// One timer supports max 4 output pins
  virtual int maxChannels() { return 4; };

  /// provides the max value for the configured resulution
  virtual int maxOutputValue() { return 10000; }

  /// write a pwm value to the indicated channel. The max value depends on the
  /// resolution
  virtual void pwmWrite(int channel, int value) {
    // analogWrite(pins[channel], value);
    pwm.setRate(channel, value);
  }

  /// timer callback: write the next frame to the pins
  static void defaultPWMAudioOutputCallback(void *obj) {
    PWMDriverSTM32 *accessAudioPWM = (PWMDriverSTM32 *)obj;
    if (accessAudioPWM != nullptr) {
      accessAudioPWM->playNextFrame();
    }
  }
};

}  // namespace audio_tools

#endif
