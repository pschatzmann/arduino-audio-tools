#pragma once

#include "AudioConfig.h"
#ifdef USE_ANALOG_ARDUINO
#include "AudioAnalog/AnalogAudioBase.h"
#include "AudioTimer/AudioTimer.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"

namespace audio_tools {

/**
 * @brief Please use the AnalogAudioStream: Reading Analog Data using a timer
 * and the Arduino analogRead() method and writing using analogWrite();
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
    if (cfg.rx_tx_mode == RXTX_MODE) {
      LOGE("RXTX not supported");
      return false;
    }
    config = cfg;
    if (buffer == nullptr) {
      // allocate buffer_count
      buffer = new NBuffer<int16_t>(cfg.buffer_size, cfg.buffer_count);
      if (buffer == nullptr) {
        LOGE("Not enough memory for buffer");
        return false;
      }
      // setup pins
      setupPins();
    } else {
      timer.end();
    }

    // (re)start timer
    LOGI("sample_rate: %d", cfg.sample_rate);
    timer.setCallbackParameter(this);
    return timer.begin(callback, cfg.sample_rate, TimeUnit::HZ);
  }

  void end() override { timer.end(); }

  int available() {
    if (config.rx_tx_mode == TX_MODE) return 0;
    return buffer == nullptr ? 0 : buffer->available() * 2;
  };

  /// Provides the sampled audio data
  size_t readBytes(uint8_t *values, size_t len) {
    if (config.rx_tx_mode == TX_MODE) return 0;
    if (buffer == nullptr) return 0;
    int samples = len / 2;
    return buffer->readArray((int16_t *)values, samples) * 2;
  }

  int availableForWrite() {
    if (config.rx_tx_mode == RX_MODE) return 0;
    return buffer == nullptr ? 0 : buffer->availableForWrite() * 2;
  }

  size_t write(const uint8_t *data, size_t len) {
    if (config.rx_tx_mode == RX_MODE) return 0;

    // blocking write ?
    if (config.is_blocking_write) {
      LOGD("Waiting for buffer to clear");
      while (buffer->availableForWrite() == 0) {
        delay(10);
      }
    }
    // write data
    int16_t *data_16 = (int16_t *)data;
    return buffer->writeArray(data_16, len / 2) * 2;
  }

 protected:
  AnalogConfig config;
  TimerAlarmRepeating timer;
  BaseBuffer<int16_t> *buffer = nullptr;
  int avg_value, min, max, count;

  /// Sample data and write to buffer
  static void callback(void *arg) {
    int16_t value = 0;
    AnalogDriverArduino *self = (AnalogDriverArduino *)arg;
    // prevent NPE
    if (self->buffer == nullptr) return;

    // Logic for reading audio data
    if (self->config.rx_tx_mode == RX_MODE) {
      int channels = self->config.channels;
      for (int j = 0; j < channels; j++) {
        // provides value in range 0…4095
        value = analogRead(self->config.start_pin + j);
        if (self->config.is_auto_center_read){
          self->updateMinMax(value);
        }
        value = (value - self->avg_value) * 16;
        self->buffer->write(value);
      }
      // Logic for writing audio data
    } else if (self->config.rx_tx_mode == TX_MODE) {
      int channels = self->config.channels;
      for (int j = 0; j < channels; j++) {
        int16_t sample = self->buffer->read();
        sample = map(sample, -32768, 32767, 0, 255);
        int pin = self->config.start_pin + j;
        analogWrite(pin, sample);
        // LOGI("analogWrite(%d, %d)", pin, sample);
      }
    }
  }

  /// pinmode input for defined analog pins
  void setupPins() {
    TRACED();
    if (config.rx_tx_mode == RX_MODE) {
      LOGI("rx start_pin: %d", config.start_pin);
      // setup pins for read
      for (int j = 0; j < config.channels; j++) {
        int pin = config.start_pin + j;
        pinMode(pin, INPUT);
        LOGD("pinMode(%d, INPUT)", pin);
      }

      if (config.is_auto_center_read){
        // calculate the avarage value to center the signal
        for (int j = 0; j < 1024; j++) {
          updateMinMax(analogRead(config.start_pin));
        }
        LOGI("Avg Signal was %d", avg_value);
      }
    } else if (config.rx_tx_mode == TX_MODE) {
      LOGI("tx start_pin: %d", config.start_pin);
      // setup pins for read
      for (int j = 0; j < config.channels; j++) {
        int pin = config.start_pin + j;
        pinMode(pin, OUTPUT);
        LOGD("pinMode(%d, OUTPUT)", pin);
      }
    }
  }

  void updateMinMax(int value){
    if (value<min) min = value;
    if (value>max) max = value;
    if (count++==1024) updateAvg();
  }

  void updateAvg(){
    avg_value = (max + min) / 2;   
    min = INT_MAX;
    max = INT_MIN; 
    count = 0;
  }
};

using AnalogDriver = AnalogDriverArduino;

}  // namespace audio_tools

#endif
