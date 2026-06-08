
#pragma once
#ifdef ESP32
#include "AudioTools/CoreAudio/AudioPWM/PWMDriverBase.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#if __has_include("soc/soc_caps.h")
#include "soc/soc_caps.h"
#endif

namespace audio_tools {

#if defined(SOC_LEDC_CHANNEL_NUM)
static constexpr int PWM_ESP32_LEDC_CHANNELS_PER_MODE = SOC_LEDC_CHANNEL_NUM;
#else
static constexpr int PWM_ESP32_LEDC_CHANNELS_PER_MODE = 8;
#endif

#if defined(SOC_LEDC_SUPPORT_HS_MODE) && SOC_LEDC_SUPPORT_HS_MODE
static constexpr int PWM_ESP32_LEDC_SPEED_MODE_COUNT = 2;
#else
static constexpr int PWM_ESP32_LEDC_SPEED_MODE_COUNT = 1;
#endif

// forward declaration
class PWMDriverESP32;
/**
 * @typedef  PWMDriverBase
 * @brief Please use PWMDriverBase!
 */
using PWMDriver = PWMDriverESP32;

/**
 * @brief Information for a PIN
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct PinInfoESP32 {
  ledc_channel_t pwm_channel;
  ledc_timer_t pwm_timer;
  ledc_mode_t speed_mode;
  int gpio;
};

using PinInfo = PinInfoESP32;

/**
 * @brief Audio output to PWM pins for the ESP32. The ESP32 supports up to 16
 * channels.
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class PWMDriverESP32 : public PWMDriverBase {
 public:
  // friend void pwm_callback(void*ptr);

  PWMDriverESP32() { TRACED(); }

  // Ends the output
  virtual void end() {
    TRACED();
    timer.end();
    is_timer_started = false;
    releasePins();
    deleteBuffer();
  }

  /// when we get the first write -> we activate the timer to start with the
  /// output of data
  virtual void startTimer() {
    if (!timer) {
      TRACEI();
      timer.begin(pwm_callback, effectiveOutputSampleRate(), HZ);
      actual_timer_frequency = effectiveOutputSampleRate();
      is_timer_started = true;
    }
  }

  /// Setup LED PWM
  virtual void setupPWM() {
    // frequency is driven by selected resolution
    if (audio_config.pwm_frequency == 0) {
      audio_config.pwm_frequency = frequency(audio_config.resolution) * 1000;
    }

    releasePins();
    pins.resize(audio_config.channels);
    bool low_speed_configured = false;
  #if defined(SOC_LEDC_SUPPORT_HS_MODE) && SOC_LEDC_SUPPORT_HS_MODE
    bool high_speed_configured = false;
  #endif
    for (int j = 0; j < audio_config.channels; j++) {
      pins[j].gpio = audio_config.pins()[j];
      pins[j].speed_mode = speedModeForChannel(j);
      pins[j].pwm_channel = ledcChannelForIndex(j);
      pins[j].pwm_timer = LEDC_TIMER_0;

      if (pins[j].speed_mode == LEDC_LOW_SPEED_MODE) {
        if (!low_speed_configured) {
          low_speed_configured = configureTimer(LEDC_LOW_SPEED_MODE);
        }
      }
#if defined(SOC_LEDC_SUPPORT_HS_MODE) && SOC_LEDC_SUPPORT_HS_MODE
      else {
        if (!high_speed_configured) {
          high_speed_configured = configureTimer(LEDC_HIGH_SPEED_MODE);
        }
      }
#endif

      configureChannel(pins[j]);
      LOGI("setupPWM: pin=%d, channel=%d, frequency=%u, resolution=%d",
           pins[j].gpio, (int)pins[j].pwm_channel,
           (unsigned)audio_config.pwm_frequency, audio_config.resolution);
    }
    logPins();
  }

  void logPins() {
    for (int j = 0; j < pins.size(); j++) {
      LOGI("pin%d: %d", j, pins[j].gpio);
    }
  }

  /// Setup ESP32 timer with callback
  virtual void setupTimer() {
    timer.setCallbackParameter(this);
    timer.setIsSave(false);

    if (actual_timer_frequency != effectiveOutputSampleRate()) {
      timer.end();
      startTimer();
    }
  }

  /// write a pwm value to the indicated channel. The max value depends on the
  /// resolution
  virtual void pwmWrite(int channel, int value) {
    if (channel < 0 || channel >= pins.size()) return;
    uint32_t duty = value;
    uint32_t max_duty = maxDutyValue();
    if (duty > max_duty) {
      duty = max_duty;
    }
    esp_err_t rc = ledc_set_duty(pins[channel].speed_mode,
                                 pins[channel].pwm_channel, duty);
    if (rc != ESP_OK) {
      LOGE("ledc_set_duty failed: pin=%d channel=%d error=%d",
           pins[channel].gpio, (int)pins[channel].pwm_channel, (int)rc);
      return;
    }
    rc = ledc_update_duty(pins[channel].speed_mode, pins[channel].pwm_channel);
    if (rc != ESP_OK) {
      LOGE("ledc_update_duty failed: pin=%d channel=%d error=%d",
           pins[channel].gpio, (int)pins[channel].pwm_channel, (int)rc);
    }
  }

 protected:
  Vector<PinInfo> pins;
  TimerAlarmRepeating timer;
  uint32_t actual_timer_frequency = 0;

  bool configureTimer(ledc_mode_t speed_mode) {
    ledc_timer_config_t config = {};
    config.speed_mode = speed_mode;
    config.timer_num = LEDC_TIMER_0;
    config.duty_resolution = resolutionBits();
    config.freq_hz = audio_config.pwm_frequency;
    config.clk_cfg = LEDC_AUTO_CLK;

    esp_err_t rc = ledc_timer_config(&config);
    if (rc != ESP_OK) {
      LOGE("ledc_timer_config failed: mode=%d error=%d", (int)speed_mode,
           (int)rc);
      return false;
    }
    return true;
  }

  void configureChannel(const PinInfo& pin) {
    ledc_channel_config_t config = {};
    config.gpio_num = pin.gpio;
    config.speed_mode = pin.speed_mode;
    config.channel = pin.pwm_channel;
    config.timer_sel = pin.pwm_timer;
    config.duty = 0;
    config.hpoint = 0;

    esp_err_t rc = ledc_channel_config(&config);
    if (rc != ESP_OK) {
      LOGE("ledc_channel_config failed: pin=%d channel=%d error=%d", pin.gpio,
           (int)pin.pwm_channel, (int)rc);
    }
  }

  void releasePins() {
    for (int j = 0; j < pins.size(); j++) {
      ledc_stop(pins[j].speed_mode, pins[j].pwm_channel, 0);
      gpio_reset_pin((gpio_num_t)pins[j].gpio);
    }
    pins.clear();
  }

  ledc_mode_t speedModeForChannel(int channel) {
#if defined(SOC_LEDC_SUPPORT_HS_MODE) && SOC_LEDC_SUPPORT_HS_MODE
    return channel < PWM_ESP32_LEDC_CHANNELS_PER_MODE ? LEDC_LOW_SPEED_MODE
                                                      : LEDC_HIGH_SPEED_MODE;
#else
    (void)channel;
    return LEDC_LOW_SPEED_MODE;
#endif
  }

  ledc_channel_t ledcChannelForIndex(int channel) {
    return static_cast<ledc_channel_t>(channel %
                                       PWM_ESP32_LEDC_CHANNELS_PER_MODE);
  }

  ledc_timer_bit_t resolutionBits() {
    return static_cast<ledc_timer_bit_t>(audio_config.resolution);
  }

  uint32_t maxDutyValue() {
    int max_value = maxOutputValue() - 1;
    return max_value > 0 ? (uint32_t)max_value : 0;
  }

  /// provides the max value for the indicated resulution
  int maxUnsignedValue(int resolution) { return pow(2, resolution); }

  virtual int maxChannels() {
    return PWM_ESP32_LEDC_CHANNELS_PER_MODE * PWM_ESP32_LEDC_SPEED_MODE_COUNT;
  };

  /// provides the max value for the configured resulution
  virtual int maxOutputValue() {
    return maxUnsignedValue(audio_config.resolution);
  }

  /// determiens the PWM frequency based on the requested resolution
  float frequency(int resolution) {
// On ESP32S2 and S3, the frequncy seems off by a factor of 2
#if defined(ESP32S2) || defined(ESP32S3)
    switch (resolution) {
      case 7:
        return 312.5;
      case 8:
        return 156.25;
      case 9:
        return 78.125;
      case 10:
        return 39.0625;
      case 11:
        return 19.53125;
    }
    return 312.5;
#else
    switch (resolution) {
      case 8:
        return 312.5;
      case 9:
        return 156.25;
      case 10:
        return 78.125;
      case 11:
        return 39.0625;
    }
    return 312.5;
#endif
  }

  /// timer callback: write the next frame to the pins
  static void pwm_callback(void* ptr) {
    PWMDriverESP32* accessAudioPWM = (PWMDriverESP32*)ptr;
    if (accessAudioPWM != nullptr) {
      accessAudioPWM->playNextFrame();
    }
  }
};

}  // namespace audio_tools

#endif
