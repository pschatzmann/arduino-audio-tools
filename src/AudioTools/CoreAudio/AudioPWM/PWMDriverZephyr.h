#pragma once

#include "AudioToolsConfig.h"

#if defined(IS_ZEPHYR)

#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#include "AudioTools/CoreAudio/AudioPWM/PWMDriverBase.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimer.h"

namespace audio_tools {

// forward declaration
class PWMDriverZephyr;

/**
 * @typedef PWMDriver
 * @brief Default PWM driver for Zephyr
 */
using PWMDriver = PWMDriverZephyr;

/**
 * @brief Audio output to PWM pins for Zephyr RTOS
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 * Supports PWM audio output on Zephyr targets using the native PWM driver API.
 * Manages multiple PWM channels with configurable frequency and resolution.
 *
 * Characteristics:
 * - Uses Zephyr's PWM driver API (pwm_set_dt, pwm_get_cycles_per_sec)
 * - Supports multiple channels via channel configuration
 * - Callback-based playback using TimerAlarmRepeating
 * - Automatic pin management
 *
 */
class PWMDriverZephyr : public PWMDriverBase {
 public:
  PWMDriverZephyr() = default;

  /// Ends PWM output
  virtual void end() override {
    TRACED();
    timer.end();  // Stop the playback timer
    is_timer_started = false;

    // Stop and release PWM devices
    for (size_t ch = 0; ch < audio_config.pins.size(); ch++) {
        int rc = pwm_set_dt(&audio_config.pins[ch], 0, 0);
        if (rc != 0) {
            LOGE("Failed to disable PWM on channel %d: %d", ch, rc);
        }
    }
    deleteBuffer();
  }

 protected:
  TimerAlarmRepeating timer;  // Callback timer for playback
  uint32_t period_ns = 0;

  /// Start the timer for periodic playback
  virtual void startTimer() override {
    if (!is_timer_started) {
      TRACED();
      // Calculate period in microseconds
      long wait_time = 1000000L / audio_config.sample_rate;
      LOGI("Starting PWM timer with period: %lu us", wait_time);

      timer.setCallbackParameter(this);
      timer.begin(defaultPWMAudioOutputCallback, wait_time, US);
      is_timer_started = true;
    }
  }

  /// Setup PWM pins and devices
  virtual void setupPWM() override {
    TRACED();

    if (audio_config.pwm_frequency == 0) {
      audio_config.pwm_frequency = PWM_AUDIO_FREQUENCY;
    }

    LOGI("Setting up %d PWM channels at %u Hz", audio_config.channels,
         audio_config.pwm_frequency);

    // pwm_dt_spec is defined as
    // struct pwm_dt_spec {
    //     const struct device *dev;
    //     uint32_t channel;
    //     uint32_t period;
    //     pwm_flags_t flags;
    // };
    int pin_count = audio_config.pins.size();
    for (int ch = 0; ch < audio_config.channels; ch++) {
        if (ch >= pin_count){
          LOGE("Not enough pins defined: pins %d, channels=%d", pin_count, audio_config.channels);
          continue;
        }

        pwm_dt_spec& spec = audio_config.pins[ch];
        if (spec.dev == nullptr) {
          LOGE("Failed to get PWM device for audio channel %d (pwm_channel %u)", ch, spec.channel);
          continue;
        }

        if (!device_is_ready(spec.dev)) {
          LOGE("Device not ready for audio channel %d (pwm_channel %u)", ch, spec.channel);
          continue;
        }

        period_ns =
            1000000000UL / audio_config.pwm_frequency;
        // Apply initial PWM configuration (0% duty cycle)
        int rc = pwm_set_dt(&spec, period_ns, 0);
        if (rc != 0) {
          LOGE("Failed to configure PWM on channel %d: %d", ch, rc);
          continue;
        }
    }

    LOGI("PWM setup complete");
  }


  /// Configure timer parameters (called after setupPWM)
  virtual void setupTimer() override {
    TRACED();
    // Timer is set up in startTimer() when needed
    // Nothing to do here
  }

  /// Maximum supported PWM channels
  virtual int maxChannels() override {
    // Zephyr PWM typically supports up to 8 channels per device
    return 8;
  }

  /// Maximum output value (resolution-dependent)
  virtual int maxOutputValue() override {
    // Using 16-bit resolution for consistency with audio data
    return 65535;
  }

  /// Write PWM value to a specific channel
  virtual void pwmWrite(int channel, int value) override {
    // prevent writes to undefined channels
    if (channel >= audio_config.pins.size()) return;
    // set duty cycle
    pwm_dt_spec spec = audio_config.pins[channel];
    // Clamp value to valid range
    uint32_t clamped_value =
        value > maxOutputValue() ? maxOutputValue() : value;

    // Calculate duty cycle in cycles
    uint32_t duty_ns =
        ((uint64_t)clamped_value * period_ns) /
        maxOutputValue();
    int rc = pwm_set_dt(&spec, period_ns, duty_ns);
    if (rc != 0) {
      LOGD("Failed to set PWM duty on channel %d: %d", channel, rc);
    }
  }

  /// Timer callback: invoked periodically to write the next audio frame
  static void defaultPWMAudioOutputCallback(void* obj) {
    PWMDriverZephyr* driver = (PWMDriverZephyr*)obj;
    if (driver != nullptr) {
      driver->playNextFrame();
    }
  }
};

}  // namespace audio_tools

#endif
