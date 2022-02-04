#pragma once

#include "AudioConfig.h"
#ifdef USE_ADC_ARDUINO
#include "AudioTimer/AudioTimer.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"

namespace audio_tools {


/**
 * @brief Configuration for Analog Reader
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AnalogConfig : public AudioBaseInfo {
 public:
  AnalogConfig() {
    channels = 1;
    sample_rate = 10000;
    bits_per_sample = 16;
  }
  int start_pin = PIN_ADC_START;
  uint16_t buffer_size = ADC_BUFFER_SIZE;
  uint8_t buffers = ADC_BUFFERS;
};

/**
 * @brief Reading Analog Data using a timer and the Arduino analogRead() method
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class AnalogAudioStream : public AudioStreamX {
 public:
  AnalogAudioStream() = default;

  AnalogConfig defaultConfig() {
    AnalogConfig cfg;
    return cfg;
  }

  void setAudioInfo(AudioBaseInfo info) { LOGW("setAudioInfo() not supported") }

  bool begin(AnalogConfig cfg) {
    LOGD("%s", __func__);
    config = cfg;
    if (buffer == nullptr) {
      // allocate buffers
      buffer = new NBuffer<int16_t>(cfg.buffer_size, cfg.buffers);
      if (buffer==nullptr){
        LOGE("Not enough memory for buffer");
        return false;
      } else {
        LOGI("buffer: %d", (int) buffer);
        LOGI("self: %d", (int) this);
      }
      // setup pins
      setupPins();
    } else {
      timer.end();
    }

    // (re)start timer
    uint32_t time = AudioUtils::toTimeUs(config.sample_rate);
    LOGI("sample_rate: %d", cfg.sample_rate);
    LOGI("time us: %u", time);
    timer.setCallbackParameter(this);
    return timer.begin(callback, time, TimeUnit::US);
  }

  virtual int available() { return buffer==nullptr ? 0 : buffer->available(); };

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

  /// Sample data and write to buffer
  static void callback(void *arg) {
    int16_t value = 0;
    AnalogAudioStream *self = (AnalogAudioStream *)arg;
    if (self->buffer != nullptr) {
      int channels = self->config.channels;
      for (int j = 0; j < channels; j++) {
        //Serial.print("+"); 
        // provides value in range 0…4095
        value = analogRead(self->config.start_pin + j);
        //Serial.print("-");
        value = (value-2048)*15;
        self->buffer->write(value);
      }
    } 
  }

  /// pinmode input for defined analog pins
  void setupPins() {
    LOGD("%s", __func__);
    LOGI("start_pin: %d", config.start_pin);
    // setup pins for read
    for (int j = 0; j < config.channels; j++) {
      int pin = config.start_pin + j;
      pinMode(pin, INPUT);
      LOGD("pinMode(%d, INPUT)",pin);
    }
  }
};

}  // namespace audio_tools

#endif