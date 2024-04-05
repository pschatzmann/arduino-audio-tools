
#pragma once
#include "AudioConfig.h"
#if defined(USE_PWM) && defined(__AVR__)
#include "AudioPWM/PWMAudioBase.h"
#include "AudioTimer/AudioTimerAVR.h"

namespace audio_tools {

class PWMDriverAVR;
using PWMDriver = PWMDriverAVR;
static PWMDriverAVR *accessAudioPWM = nullptr;

/**
 * @brief Experimental: Audio output to PWM pins for the AVR. The AVR supports
 * only up to 2 channels.
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class PWMDriverAVR : public DriverPWMBase {
  friend void defaultPWMAudioOutputCallback();

 public:
  PWMDriverAVR() {
    LOGD("PWMDriverAVR");
    accessAudioPWM = this;
  }

  virtual int maxChannels() { return 2; };

  // Ends the output
  virtual void end() {
    TRACED();
    noInterrupts();
    // stop timer callback
    TCCR1B = 0;
    // stop pwm timers
    TCCR2A = 0;
    interrupts();  // enable all interrupts

    is_timer_started = false;
    deleteBuffer();
  }

  void setupTimer() {
    TRACED();
    // CPU Frequency 16 MHz
    // prescaler 1, 256 or 1024 => no prescaling
    uint32_t steps =
        F_CPU / 8 / audio_config.sample_rate;  // e.g. (16000000/8/44100=>45)
    if (steps > 65535) {
      LOGE("requested sample rate not supported: %d - we use %d",
           audio_config.sample_rate, F_CPU / 65536);
      steps = 65535;
    } else {
      LOGD("compare match register set to %d", steps);
    }

    // setup timer intterupt
    noInterrupts();
    TCCR1B = 0;
    // compare match register
    OCR1A = steps;
    TCCR1B |= (1 << WGM12);  // CTC mode
    // TCCR1B |= (1 << CS10); // prescaler 1
    TCCR1B |= (1 << CS11);    // prescaler 8
    TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
    interrupts();             // enable all interrupts
  }

  /// Setup LED PWM
  void setupPWM() {
    TRACED();
    if (audio_config.channels > 2) {
      LOGW("Max 2 channels supported - you requested %d",
           audio_config.channels);
      audio_config.channels = 2;
    }

    for (int j = 0; j < audio_config.channels; j++) {
      LOGD("Processing channel %d", j);
      setupPin(pins[j]);
    }
  }

  void startTimer() {}

  // Timer 0 is used by Arduino!
  // Timer 1 is used to drive output in sample_rate
  // => only Timer2 is available for PWM
  void setupPin(int pin) {
    switch (pin) {
      case 3:
      case 11:
        // switch PWM frequency to 62500.00 Hz
        TCCR2B = TCCR2B & B11111000 | B00000001;
        LOGI("PWM Frequency changed for D3 and D11");
        break;

      default:
        LOGE("PWM Unsupported pin: %d", pin);
        break;
    }
    pinMode(pin, OUTPUT);
  }

  virtual void pwmWrite(int channel, int value) {
    analogWrite(pins[channel], value);
  }

  void logConfig() {
    audio_config.logConfig();
    LOGI("pwm freq: %f khz", 62.5);
    if (audio_config.channels == 1) {
      LOGI("output pin: %d", pins[0]);
    } else {
      LOGI("output pins: %d / %d", pins[0], pins[1]);
    }
  }

 protected:
  int pins[2] = {3, 11};

  virtual int maxOutputValue() { return 255; }
};

/// separate method that can be defined as friend so that we can access
/// protected information
void defaultPWMAudioOutputCallback() {
  if (accessAudioPWM != nullptr && accessAudioPWM->is_timer_started) {
    accessAudioPWM->playNextFrame();
  }
}

/// timer callback: write the next frame to the pins
ISR(TIMER1_COMPA_vect) {
  defaultPWMAudioOutputCallback();
  TimerAlarmRepeatingDriverAVR::tickerCallback();
}

}  // namespace audio_tools

#endif
