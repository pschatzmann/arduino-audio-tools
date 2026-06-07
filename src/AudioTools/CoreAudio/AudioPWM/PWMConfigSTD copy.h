#pragma once

namespace audio_tools {
/**
 * @brief Configuration data for PWM audio output
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @ingroup platform
 */
struct PWMConfigSTD : public AudioInfo {
  PWMConfigSTD() {
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

  /// GPIO of starting pin
  uint16_t start_pin = PIN_PWM_START;

  /// support assignament of int array
  template <typename T, int N>
  void setPins(T (&a)[N]) {
    pins_data.clear();
    pins_data.resize(N);
    for (int i = 0; i < N; ++i) {
      pins_data[i] = a[i];  // reset all elements
    }
  }

  /// Defines the pins and the corresponding number of channels (=number of
  /// pins)
  void setPins(Pins &pins) {
    pins_data.clear();
    for (int i = 0; i < pins.size(); i++) {
      pins_data.push_back(pins[i]);
    }
  }

  /// Determines the pins (for all channels)
  Pins &pins() {
    if (pins_data.size() == 0) {
      pins_data.resize(channels);
      for (int j = 0; j < channels; j++) {
        pins_data[j] = start_pin + j;
      }
    }
    return pins_data;
  }


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
