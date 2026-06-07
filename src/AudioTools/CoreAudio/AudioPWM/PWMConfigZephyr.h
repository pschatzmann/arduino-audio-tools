#pragma once

#include "stdint.h"
#include "vector"
#include <zephyr/drivers/pwm.h>

/// Macro to get the pwm_dt_spec
#define PWM_PIN(pin_label) PWM_DT_SPEC_GET(DT_NODELABEL(pin_label))

namespace audio_tools {
/**
 * @brief Configuration data for PWM audio output. Define the pins
 * as follows:
 *   config.pins = {PWM_PIN(pwm_led_0), PWM_PIN(pwm_led_2)};
 * which is a short for for the zephyr standard macros:
 *   config.pins = {PWM_DT_SPEC_GET(DT_NODELABEL(pwm_led_0)), PWM_DT_SPEC_GET(DT_NODELABEL(pwm_led_1))};
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct PWMConfigZephyr : public AudioInfo {
  PWMConfigZephyr() {
    // default basic information
    sample_rate = 8000u;  // sample rate in Hz
    channels = 1;
    bits_per_sample = 16;
  }

  /// size of an inidividual buffer
  uint16_t buffer_size = PWM_BUFFER_SIZE;
  /// number of buffers
  uint8_t buffers = PWM_BUFFER_COUNT;

  uint32_t pwm_frequency = 0;  // audable range is from 20 to

  uint8_t resolution = 8;

  /// max sample sample rate that still produces good audio
  uint32_t max_sample_rate = PWM_MAX_SAMPLE_RATE;

  /// Define the pwm pins in zephyr
  std::vector<pwm_dt_spec> pins;

  void logConfig() {
    LOGI("sample_rate: %d", (int) sample_rate);
    LOGI("channels: %d", channels);
    LOGI("bits_per_sample: %u", bits_per_sample);
    LOGI("buffer_size: %u", buffer_size);
    LOGI("buffer_count: %u", buffers);
    LOGI("pwm_frequency: %u", (unsigned)pwm_frequency);
    LOGI("resolution: %d", resolution);
    LOGI("pwm pin count: %d", pwm_pins.size());
  }
};

using PWMConfig = PWMConfigZephyr;

}
