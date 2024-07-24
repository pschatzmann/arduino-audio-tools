#pragma once

#include "AudioConfig.h"
#ifdef USE_PWM

#include "AudioBasic/Collections.h"
#include "AudioTools.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioOutput.h"
#include "AudioTools/AudioTypes.h"

#define READ_ERROR_MSG "Could not read full data"

namespace audio_tools {

// forward declarations
// Callback for user
typedef bool (*PWMCallbackType)(uint8_t channels, int16_t *data);
// Callback used by system
static void defaultPWMAudioOutputCallback();
// Driver classes
class PWMDriverESP32;
class PWMDriverRP2040;
class PWMDriverMBED;
class PWMDriverSTM32;

/**
 * @brief Configuration data for PWM audio output
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct PWMConfig : public AudioInfo {
  PWMConfig() {
    // default basic information
    sample_rate = 8000u;  // sample rate in Hz
    channels = 1;
    bits_per_sample = 16;
  }

  // basic pwm information
  uint16_t buffer_size = PWM_BUFFER_SIZE;
  uint8_t buffers = PWM_BUFFER_COUNT;

  // additinal info which might not be used by all processors
  uint32_t pwm_frequency = PWM_AUDIO_FREQUENCY;  // audable range is from 20 to
                                                 // 20,000Hz (not used by ESP32)
  uint8_t resolution = 8;  // Only used by ESP32: must be between 8 and 11 ->
                           // drives pwm frequency
  uint8_t timer_id = 0;    // Only used by ESP32 must be between 0 and 3

#ifndef __AVR__
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

#endif

  void logConfig() {
    LOGI("sample_rate: %d", (int) sample_rate);
    LOGI("channels: %d", channels);
    LOGI("bits_per_sample: %u", bits_per_sample);
    LOGI("buffer_size: %u", buffer_size);
    LOGI("buffer_count: %u", buffers);
    LOGI("pwm_frequency: %u", (unsigned)pwm_frequency);
    LOGI("resolution: %d", resolution);
    // LOGI("timer_id: %d", timer_id);
  }

 protected:
  Pins pins_data;
};

/**
 * @brief Base Class for all PWM drivers
 *
 */
class DriverPWMBase {
 public:
  DriverPWMBase() = default;
  virtual ~DriverPWMBase() { end(); }

  PWMConfig &audioInfo() { return audio_config; }

  virtual PWMConfig defaultConfig() {
    PWMConfig cfg;
    return cfg;
  }

  // restart with prior definitions
  bool begin(PWMConfig cfg) {
    TRACED();
    audio_config = cfg;
    decimate.setChannels(cfg.channels);
    decimate.setBits(cfg.bits_per_sample);
    decimate.setFactor(decimation());
    LOGI("decimation: %d", decimation());
    frame_size = audio_config.channels * (audio_config.bits_per_sample / 8);
    if (audio_config.channels > maxChannels()) {
      LOGE("Only max %d channels are supported!", maxChannels());
      return false;
    }

    if (buffer == nullptr) {
      LOGI("->Allocating new buffer %d * %d bytes", audio_config.buffers,
           audio_config.buffer_size);
      // buffer = new NBuffer<uint8_t>(audio_config.buffer_size,
      // audio_config.buffers);
      buffer = new RingBuffer<uint8_t>(audio_config.buffer_size *
                                       audio_config.buffers);
    }

    // initialize if necessary
    if (!isTimerStarted() || !cfg.equals(actual_info)) {
      audio_config.logConfig();
      setupPWM();
      setupTimer();
      actual_info = cfg;
    }

    // reset class variables
    underflow_count = 0;
    underflow_per_second = 0;
    frame_count = 0;
    frames_per_second = 0;

    LOGI("->Buffer available: %d", buffer->available());
    LOGI("->Buffer available for write: %d", buffer->availableForWrite());
    LOGI("->is_timer_started: %s ", isTimerStarted() ? "true" : "false");
    return true;
  }

  virtual int availableForWrite() {
    return is_blocking_write
               ? audio_config.buffer_size
               : buffer->availableForWrite() / frame_size * frame_size;
  }

  // blocking write for an array: we expect a singed value and convert it into a
  // unsigned
  virtual size_t write(const uint8_t *data, size_t len) {
    size_t size = len;

    // only allow full frame
    size = (size / frame_size) * frame_size;
    LOGD("adjusted size: %d", (int)size);

    if (isDecimateActive()) {
      size = decimate.convert((uint8_t *)data, size);
      LOGD("decimated size: %d", (int)size);
    }

    if (is_blocking_write && buffer->availableForWrite() < size) {
      LOGD("Waiting for buffer to be available");
      while (buffer->availableForWrite() < size) delay(5);
    } else {
      size = min((size_t)availableForWrite(), size);
    }

    size_t result = buffer->writeArray(data, size);
    if (result != size) {
      LOGW("Could not write all data: %u -> %d", (unsigned int)size, result);
    }
    // activate the timer now - if not already done
    if (!is_timer_started) startTimer();
    return result * decimation();
  }

  // When the timer does not have enough data we increase the underflow_count;
  uint32_t underflowsPerSecond() { return underflow_per_second; }
  // provides the effectivly measured output frames per second
  uint32_t framesPerSecond() { return frames_per_second; }

  inline void updateStatistics() {
    frame_count++;
    if (millis() >= time_1_sec) {
      time_1_sec = millis() + 1000;
      frames_per_second = frame_count;
      underflow_per_second = underflow_count;
      underflow_count = 0;
      frame_count = 0;
    }
  }

  bool isTimerStarted() { return is_timer_started; }

  virtual void setupPWM() = 0;
  virtual void setupTimer() = 0;
  virtual void startTimer() = 0;
  virtual int maxChannels() = 0;
  virtual int maxOutputValue() = 0;
  virtual void end() {};

  virtual void pwmWrite(int channel, int value) = 0;

  /// You can assign your own custom buffer impelementation: must be allocated
  /// on the heap and will be cleaned up by this class
  void setBuffer(BaseBuffer<uint8_t> *buffer) { this->buffer = buffer; }

 protected:
  PWMConfig audio_config;
  AudioInfo actual_info;
  BaseBuffer<uint8_t> *buffer = nullptr;
  uint32_t underflow_count = 0;
  uint32_t underflow_per_second = 0;
  uint32_t frame_count = 0;
  uint32_t frames_per_second = 0;
  uint32_t frame_size = 0;
  uint32_t time_1_sec;
  bool is_timer_started = false;
  bool is_blocking_write = true;
  Decimate decimate;

  void deleteBuffer() {
    // delete buffer if necessary
    if (buffer != nullptr) {
      delete buffer;
      buffer = nullptr;
    }
  }

  /// writes the next frame to the output pins
  void playNextFrame() {
    if (isTimerStarted() && buffer != nullptr) {
      // TRACED();
      int required = (audio_config.bits_per_sample / 8) * audio_config.channels;
      if (buffer->available() >= required) {
        for (int j = 0; j < audio_config.channels; j++) {
          int value = nextValue();
          pwmWrite(j, value);
        }
      } else {
        underflow_count++;
      }
      updateStatistics();
    }
  }

  /// determines the next scaled value
  virtual int nextValue() {
    int result = 0;
    switch (audio_config.bits_per_sample) {
      case 8: {
        int16_t value = buffer->read();
        if (value < 0) {
          LOGE(READ_ERROR_MSG);
          value = 0;
        }
        result = map(value, -NumberConverter::maxValue(8),
                     NumberConverter::maxValue(8), 0, maxOutputValue());
        break;
      }
      case 16: {
        int16_t value;
        if (buffer->readArray((uint8_t *)&value, 2) != 2) {
          LOGE(READ_ERROR_MSG);
        }
        result = map(value, -NumberConverter::maxValue(16),
                     NumberConverter::maxValue(16), 0, maxOutputValue());
        break;
      }
      case 24: {
        int24_t value;
        if (buffer->readArray((uint8_t *)&value, 3) != 3) {
          LOGE(READ_ERROR_MSG);
        }
        result = map((int32_t)value, -NumberConverter::maxValue(24),
                     NumberConverter::maxValue(24), 0, maxOutputValue());
        break;
      }
      case 32: {
        int32_t value;
        if (buffer->readArray((uint8_t *)&value, 4) != 4) {
          LOGE(READ_ERROR_MSG);
        }
        result = map(value, -NumberConverter::maxValue(32),
                     NumberConverter::maxValue(32), 0, maxOutputValue());
        break;
      }
    }
    return result;
  }

  /// Provides the max working sample rate
  virtual int maxSampleRate() { return 48000; }

  /// The requested sampling rate is too hight: we only process half of the
  /// samples so we can half the sampling rate
  virtual bool isDecimateActive() {
    return audio_config.sample_rate >= ANALOG_MAX_SAMPLE_RATE;
  }

  /// Provides the effective sample rate
  virtual int effectiveOutputSampleRate() {
    return audio_config.sample_rate / decimation();
  }

  /// Decimation factor to reduce the sample rate
  virtual int decimation() { return 1; }
};

}  // namespace audio_tools

#endif
