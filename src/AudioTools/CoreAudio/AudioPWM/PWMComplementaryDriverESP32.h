
#pragma once
#ifdef ESP32
#include "AudioTools/CoreAudio/AudioPWM/PWMDriverBase.h"
#include "driver/mcpwm.h"

namespace audio_tools {

/**
 * @brief Mapping information for one complementary PWM audio channel.
 *
 * A single logical audio PWM channel is produced by one MCPWM timer within an
 * MCPWM unit. Each timer provides two generator outputs (A and B) which we map
 * to a high-side MOSFET gate (gpio_high / PWMxA) and a low-side MOSFET gate
 * (gpio_low / PWMxB). When dead time is enabled the peripheral inserts blanking
 * around switching edges to avoid shoot-through between the high and low
 * transistors on a half bridge.
 */
struct PinInfoESP32Compl {
  int gpio_high;          // high-side pin (PWMxA)
  int gpio_low;           // low-side pin (PWMxB)
  mcpwm_unit_t unit;      // 0..1
  mcpwm_timer_t timer;    // 0..2
};

/**
 * @brief Audio output to PWM pins for the ESP32. The ESP32 supports up to 16
 * channels.
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

/**
 * @class PWMComplementaryDriverESP32
 * @brief Complementary (half‑bridge) PWM audio driver for ESP32 using the legacy MCPWM driver API.
 *
 * @details This driver produces audio by modulating one or more complementary
 * PWM pairs (high / low outputs) using the ESP32 MCPWM peripheral. Each audio
 * channel occupies one MCPWM timer (up to 3 timers per MCPWM unit, and 2 units
 * → maximum of 6 complementary channels). For every channel two GPIOs are
 * driven 180° out of phase. Optional hardware dead time can be inserted to
 * protect external half‑bridge power stages.
 *
 * The duty cycle is derived from the (optionally decimated) audio samples in
 * the base class buffer. The effective output sample rate equals the rate at
 * which new duty values are pushed to the MCPWM hardware (timer interrupt /
 * alarm frequency) and may differ from the nominal input sample rate due to
 * decimation (see DriverPWMBase).
 *
 * Resolution & PWM Frequency:
 * The requested bit resolution (e.g. 8–11 bits) determines the PWM carrier
 * frequency using empirically chosen values (see frequency()). If
 * PWMConfig::pwm_frequency is left as 0 the driver derives it from the
 * resolution. Otherwise the provided frequency is used.
 *
 * Dead Time:
 * If PWMConfig::dead_time_us > 0 the driver configures hardware complementary
 * mode with symmetric dead time (rising & falling) in ticks computed from the
 * APB clock (assumed 80 MHz). For very large dead time requests relative to
 * the PWM period the value is limited to maintain a usable duty range.
 * Without dead time the driver emulates complementary operation by inverting
 * the B duty in software (B = 100% - A).
 *
 * Pin Assignment:
 * Provide either 2 * channels GPIO values (high0, low0, high1, low1, ...). If
 * only one pin per channel is supplied the low pin is assumed to be high+1.
 *
 * Limitations:
 * - Uses the legacy MCPWM API (driver/mcpwm.h). ESP-IDF v5+ introduces a new
 *   prelude API which is not covered here.
 * - Maximum 6 complementary channels (2 units * 3 timers).
 * - Dead time calculation assumes 80 MHz APB clock.
 * - No dynamic reconfiguration of frequency / dead time while running; call
 *   end() and begin() again to change.
 *
 * Thread Safety:
 * Access from a single task context; ISR only invokes playNextFrame().
 * 
 * @ingroup platform
 */
class PWMComplementaryDriverESP32 : public DriverPWMBase {
 public:
  // friend void pwm_callback(void*ptr);

  PWMComplementaryDriverESP32() { TRACED(); }

  // Ends the output
  virtual void end() {
    TRACED();
    timer.end();
    is_timer_started = false;
    for (int j = 0; j < pins.size(); j++) {
      mcpwm_stop(pins[j].unit, pins[j].timer);
    }
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

  /// Setup complementary MCPWM
  virtual void setupPWM() {
    // frequency is driven by selected resolution
    if (audio_config.pwm_frequency == 0) {
      audio_config.pwm_frequency = frequency(audio_config.resolution) * 1000;
    }
    if (audio_config.channels > maxChannels()) {
      LOGE("Only %d complementary channels supported", maxChannels());
      audio_config.channels = maxChannels();
    }
    bool has_pairs = audio_config.pins().size() >= (size_t)(audio_config.channels * 2);
    if (!has_pairs) {
      LOGW("Expected %d pins for %d complementary channels, got %d - assuming consecutive pin+1 as low-side", audio_config.channels*2, audio_config.channels, audio_config.pins().size());
    }
    pins.resize(audio_config.channels);
    for (int j = 0; j < audio_config.channels; j++) {
      pins[j].unit = (mcpwm_unit_t)(j / 3);
      pins[j].timer = (mcpwm_timer_t)(j % 3);
      if (pins[j].unit > MCPWM_UNIT_1) { LOGE("Too many channels for MCPWM: %d", j); break; }
      if (has_pairs) {
        pins[j].gpio_high = audio_config.pins()[j*2];
        pins[j].gpio_low  = audio_config.pins()[j*2 + 1];
      } else {
        pins[j].gpio_high = audio_config.pins()[j];
        pins[j].gpio_low  = pins[j].gpio_high + 1;
      }
      mcpwm_io_signals_t sigA = (mcpwm_io_signals_t)(MCPWM0A + (pins[j].timer * 2));
      mcpwm_io_signals_t sigB = (mcpwm_io_signals_t)(MCPWM0B + (pins[j].timer * 2));
      esp_err_t err = mcpwm_gpio_init(pins[j].unit, sigA, pins[j].gpio_high);
      if (err != ESP_OK) LOGE("mcpwm_gpio_init high error=%d", (int)err);
      err = mcpwm_gpio_init(pins[j].unit, sigB, pins[j].gpio_low);
      if (err != ESP_OK) LOGE("mcpwm_gpio_init low error=%d", (int)err);
      mcpwm_config_t cfg; cfg.frequency = audio_config.pwm_frequency; cfg.cmpr_a = 0; cfg.cmpr_b = 0; cfg.counter_mode = MCPWM_UP_COUNTER; cfg.duty_mode = MCPWM_DUTY_MODE_0;
      err = mcpwm_init(pins[j].unit, pins[j].timer, &cfg);
      if (err != ESP_OK) LOGE("mcpwm_init error=%d", (int)err);
      if (audio_config.dead_time_us > 0) {
        uint32_t dead_ticks = audio_config.dead_time_us * 80u; // 80MHz APB
        uint32_t period_ticks = 80000000UL / audio_config.pwm_frequency;
        if (dead_ticks * 2 >= period_ticks) dead_ticks = period_ticks / 4;
        if (dead_ticks > 0) {
          err = mcpwm_deadtime_enable(pins[j].unit, pins[j].timer, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, dead_ticks, dead_ticks);
          if (err != ESP_OK) LOGE("deadtime_enable error=%d", (int)err);
        }
      }
      LOGI("Complementary PWM ch=%d unit=%d timer=%d high=%d low=%d freq=%u dead_us=%u", j,(int)pins[j].unit,(int)pins[j].timer,pins[j].gpio_high,pins[j].gpio_low,(unsigned)audio_config.pwm_frequency,(unsigned)audio_config.dead_time_us);
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
    int duty = (int)((int64_t)value * 100 / maxOutputValue());
    if (duty < 0) duty = 0; else if (duty > 100) duty = 100;
    mcpwm_set_duty(pins[channel].unit, pins[channel].timer, MCPWM_OPR_A, duty);
    mcpwm_set_duty_type(pins[channel].unit, pins[channel].timer, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    if (audio_config.dead_time_us == 0) {
      // software complementary: invert
      mcpwm_set_duty(pins[channel].unit, pins[channel].timer, MCPWM_OPR_B, 100 - duty);
      mcpwm_set_duty_type(pins[channel].unit, pins[channel].timer, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
    }
  }

 protected:
  Vector<PinInfoESP32Compl> pins;
  TimerAlarmRepeating timer;
  uint32_t actual_timer_frequency = 0;

  /// provides the max value for the indicated resulution
  int maxUnsignedValue(int resolution) { return pow(2, resolution); }

  virtual int maxChannels() { return 6; };

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
  static void pwm_callback(void *ptr) {
    PWMComplementaryDriverESP32 *drv = (PWMComplementaryDriverESP32 *)ptr;
    if (drv != nullptr) {
      drv->playNextFrame();
    }
  }
};

}  // namespace audio_tools

#endif
