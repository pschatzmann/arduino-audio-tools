#pragma once

namespace audio_tools {
/**
 * @brief Configuration data for PWM audio output
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @ingroup platform
 */
struct PWMConfigAVR : public AudioInfo {
  PWMConfigAVR() {
    // default basic information
    sample_rate = 8000u;  // sample rate in Hz
    channels = 1;
    bits_per_sample = 16;
  }

  /// size of an inidividual buffer
  uint16_t buffer_size = PWM_BUFFER_SIZE;
  /// number of buffers
  uint8_t buffers = PWM_BUFFER_COUNT;

  /// additinal info which might not be used by all processors
  uint32_t pwm_frequency = 0;  // audable range is from 20 to
  /// Only used by ESP32: must be between 8 and 11 ->  drives pwm frequency                                              // 20,000Hz (not used by ESP32)
  uint8_t resolution = 8;
  /// Timer used: Only used by ESP32 must be between 0 and 3
  uint8_t timer_id = 0;

  /// Dead time in microseconds for symmetric PWM (ESP32 only)
  uint16_t dead_time_us = 0;

  /// max sample sample rate that still produces good audio
  uint32_t max_sample_rate = PWM_MAX_SAMPLE_RATE;


  void logConfig() {
    LOGI("sample_rate: %d", (int) sample_rate);
    LOGI("channels: %d", channels);
    LOGI("bits_per_sample: %u", bits_per_sample);
    LOGI("buffer_size: %u", buffer_size);
    LOGI("buffer_count: %u", buffers);
    LOGI("pwm_frequency: %u", (unsigned)pwm_frequency);
    LOGI("resolution: %d", resolution);
    LOGI("dead_time_us: %u", dead_time_us);
    // LOGI("timer_id: %d", timer_id);
  }

 protected:
  Pins pins_data;
};

using PWMConfig = PWMConfigAVR;

}
