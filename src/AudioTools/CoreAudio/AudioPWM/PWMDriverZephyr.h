#pragma once

#include "AudioToolsConfig.h"

#if defined(IS_ZEPHYR) || defined(DOXYGEN)

#include "AudioTools/CoreAudio/AudioPWM/PWMDriverBase.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimer.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>

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
class PWMDriverZephyr : public DriverPWMBase {
 public:
  PWMDriverZephyr() = default;

  /// Ends PWM output
  virtual void end() override {
    TRACED();
    timer.end();  // Stop the playback timer
    is_timer_started = false;

    // Stop and release PWM devices
    for (int j = 0; j < audio_config.channels; j++) {
      if (pwm_devices[j] != nullptr) {
                // Disable PWM on this channel
        struct pwm_dt_spec spec = {.dev = pwm_devices[j]->dev,
                                    .channel = pwm_devices[j]->channel,
                                    .flags = 0};
        int rc = pwm_set_dt(&spec, 0, 0);
        if (rc != 0) {
          LOGE("Failed to disable PWM on channel %d: %d", j, rc);
        }
      }
    }
    pwm_devices.clear();
    deleteBuffer();
  }

 protected:
  /// PWM device configuration wrapper
  struct PWMDeviceInfo {
    const struct device* dev;
    uint32_t channel;
    uint32_t period_cycles;
  };

  Vector<PWMDeviceInfo*> pwm_devices;
  TimerAlarmRepeating timer;  // Callback timer for playback

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

    pwm_devices.resize(audio_config.channels);

    for (int j = 0; j < audio_config.channels; j++) {
      uint32_t gpio_pin = audio_config.pins()[j];

      // Get the PWM device (simplified - in real Zephyr you'd use devicetree)
      // For this example, we assume pwm0 is available
      const struct device* pwm_dev = device_get_binding("PWM_0");
      if (!pwm_dev) {
        LOGE("Failed to get PWM device for channel %d (pin %u)", j, gpio_pin);
        continue;
      }

      if (!device_is_ready(pwm_dev)) {
        LOGE("PWM device not ready for channel %d", j);
        continue;
      }

      // Create device info
      auto dev_info = new PWMDeviceInfo();
      dev_info->dev = pwm_dev;
      dev_info->channel = j;  // Typically maps to PWM channel
      uint64_t cycles_per_sec = 0;
      pwm_get_cycles_per_sec(pwm_dev, 0, &cycles_per_sec);
      dev_info->period_cycles = (uint32_t)(cycles_per_sec / audio_config.pwm_frequency);

      LOGI("PWM Channel %d: pin=%u, period_cycles=%u", j, gpio_pin,
           dev_info->period_cycles);

      // Apply initial PWM configuration (0% duty cycle)
      struct pwm_dt_spec spec = {.dev = pwm_dev,
                                  .channel = (uint32_t)j,
                                  .flags = 0};
      int rc = pwm_set_dt(&spec, dev_info->period_cycles, 0);
      if (rc != 0) {
        LOGE("Failed to configure PWM on channel %d: %d", j, rc);
        delete dev_info;
        continue;
      }

      pwm_devices[j] = dev_info;
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
    if (channel < 0 || channel >= pwm_devices.size() ||
        pwm_devices[channel] == nullptr) {
      LOGW("Invalid PWM channel: %d", channel);
      return;
    }

    PWMDeviceInfo* dev_info = pwm_devices[channel];

    // Clamp value to valid range
    uint32_t clamped_value = value > maxOutputValue() ? maxOutputValue() : value;

    // Calculate duty cycle in cycles
    uint32_t duty_cycles =
        (clamped_value * dev_info->period_cycles) / maxOutputValue();

    struct pwm_dt_spec spec = {.dev = dev_info->dev,
                                .channel = dev_info->channel,
                                .flags = 0};

    int rc = pwm_set_dt(&spec, dev_info->period_cycles, duty_cycles);
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
