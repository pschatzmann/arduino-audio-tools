#pragma once

#include "AudioConfig.h"
#ifdef USE_ADC_ARDUINO
#include "AudioAnalog/AnalogAudioBase.h"
#include "AudioTimer/AudioTimer.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"

namespace audio_tools {

/**
 * @brief Please use the AnalogAudioStream: Reading Analog Data using a timer and the Arduino analogRead() method
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class AnalogDriverArduino : public AnalogDriverBase {
 public:
  AnalogDriverArduino() = default;

  bool begin(AnalogConfig cfg) {
    TRACED();
    config = cfg;
    if (buffer == nullptr) {
      // allocate buffer_count
      buffer = new NBuffer<int16_t>(cfg.buffer_size, cfg.buffer_count);
      if (buffer==nullptr){
        LOGE("Not enough memory for buffer");
        return false;
      } 
      // setup pins
      setupPins();
    } else {
      timer.end();
    }

    // (re)start timer
    uint32_t time = AudioTime::toTimeUs(config.sample_rate);
    LOGI("sample_rate: %d", cfg.sample_rate);
    LOGI("time us: %lu", time);
    timer.setCallbackParameter(this);
    return timer.begin(callback, time, TimeUnit::US);
  }

  int available() { return buffer==nullptr ? 0 : buffer->available()*2; };

  /// Provides the sampled audio data
  size_t readBytes(uint8_t *values, size_t len) {
    if (buffer==nullptr) return 0;
    int samples = len / 2;
    return buffer->readArray((int16_t *)values, samples);
  }

 protected:
  AnalogConfig config;
  TimerAlarmRepeating timer;
  BaseBuffer<int16_t> *buffer = nullptr;
  int16_t avg_value=0;

  /// Sample data and write to buffer
  static void callback(void *arg) {
    int16_t value = 0;
    AnalogDriverArduino *self = (AnalogDriverArduino *)arg;
    if (self->buffer != nullptr) {
      int channels = self->config.channels;
      for (int j = 0; j < channels; j++) {
        // provides value in range 0…4095
        value = analogRead(self->config.start_pin + j);
        value = (value - self->avg_value) * 16;
        self->buffer->write(value);
      }
    } 
  }

  /// pinmode input for defined analog pins
  void setupPins() {
    TRACED();
    LOGI("start_pin: %d", config.start_pin);
    // setup pins for read
    for (int j = 0; j < config.channels; j++) {
      int pin = config.start_pin + j;
      pinMode(pin, INPUT);
      LOGD("pinMode(%d, INPUT)",pin);
    }
    // calculate the avarage value to center the signal
    float total=0;
    for (int j=0;j<200;j++){
      total += analogRead(config.start_pin);
    }
    avg_value = total / 200.0;
    LOGI("Avg Signal was %d", avg_value);

  }
};

using AnalogDriver = AnalogDriverArduino;

}  // namespace audio_tools

#endif