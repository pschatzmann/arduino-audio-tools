#pragma once

#include <limits.h>  // for INT_MIN and INT_MAX

#include "AnalogConfigStd.h"
#include "AudioTools/CoreAudio/AudioAnalog/AnalogDriverBase.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimer.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

/**
 * @brief Analog Data IO using a timer
 * and the Arduino analogRead() method and writing using analogWrite();
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class AnalogAudioArduino : public AudioStream {
 public:
  AnalogAudioArduino() = default;

  /// provides the default configuration
  AnalogConfigStd defaultConfig() {
    AnalogConfigStd def;
    return def;
  }

  void setAudioInfo(AudioInfo info) override {
    TRACEI();
    if (config.sample_rate != info.sample_rate ||
        config.channels != info.channels ||
        config.bits_per_sample != info.bits_per_sample) {
      config.sample_rate = info.sample_rate;
      config.bits_per_sample = info.bits_per_sample;
      config.channels = info.channels;
      config.logInfo();
      setupTimer();
    }
  }

  /// Reopen with last config
  bool begin() override { return begin(config); }

  bool begin(AnalogConfigStd cfg) {
    TRACED();

    config = cfg;
    if (config.rx_tx_mode == RXTX_MODE) {
      LOGE("RXTX not supported");
      return false;
    }

    frame_size = config.channels * (config.bits_per_sample / 8);
    result_factor = 1;

    if (!setupPins()) return false;

    if (!setupTx()) return false;

    if (!setupBuffer()) return false;

    // (re)start timer
    return setupTimer();
  }

  void end() override { timer.end(); }

  int available() override {
    if (config.rx_tx_mode == TX_MODE) return 0;
    return buffer == nullptr ? 0 : buffer->available() * 2;
  };

  /// Provides the sampled audio data
  size_t readBytes(uint8_t *data, size_t len) override {
    if (config.rx_tx_mode == TX_MODE) return 0;
    if (buffer == nullptr) return 0;
    int bytes = len / frame_size * frame_size;
    return buffer->readArray(data, bytes);
  }

  int availableForWrite() override {
    if (config.rx_tx_mode == RX_MODE) return 0;
    if (buffer == nullptr) return 0;
    return config.is_blocking_write ? config.buffer_size
                                    : buffer->availableForWrite();
  }

  size_t write(const uint8_t *data, size_t len) override {
    LOGD("write: %d", (int)len);
    if (config.rx_tx_mode == RX_MODE) return 0;
    // only process full frames
    len = len / frame_size * frame_size;

    if (isCombinedChannel()) {
      ChannelReducer cr(1, 2, config.bits_per_sample);
      len = cr.convert((uint8_t *)data, len);
      LOGD("ChannelReducer len: %d", (int)len);
    }

    if (isDecimateActive()) {
      Decimate dec(decim, 1, config.bits_per_sample);
      len = dec.convert((uint8_t *)data, len);
      LOGD("Decimate len: %d for factor %d", (int)len, decim);
    }

    // blocking write ?
    if (config.is_blocking_write) {
      LOGD("Waiting for buffer to be available");
      while (buffer->availableForWrite() < len) {
        delay(10);
      }
    }

    size_t result = 0;;
    switch(config.bits_per_sample){
      case 8: {
        result = buffer->writeArray(data, len);
        } break;
      case 16: {
        size_t samples = len / 2;
        int16_t *p16 = (int16_t*)data;
        for (int j=0;j<samples;j++){
          uint8_t sample = map(p16[j],-32768, 32767,0,255);
          if (buffer->write(sample)){
            result += 2;
          } else {
            break;            
          }
        }
      } break;
      case 24: {
        size_t samples = len / 3;
        int24_t *p24 = (int24_t*)data;
        for (int j=0;j<samples;j++){
          uint8_t sample = map(p24[j],-8388608, 8388607,0,255);
          if (buffer->write(sample)){
            result += 3;
          } else {
            break;            
          }
        }

      } break;
      case 32: {
        size_t samples = len / 4;
        int32_t *p32 = (int32_t*)data;
        for (int j=0;j<samples;j++){
          uint8_t sample = map(p32[j],-2147483648, 2147483647,0,255);
          if (buffer->write(sample)){
            result += 4;
          } else {
            break;            
          }
        }

      } break;
    }

    // write data
    return result * result_factor;
  }

 protected:
  AnalogConfigStd config;
  TimerAlarmRepeating timer;
  BaseBuffer<uint8_t> *buffer = nullptr;
  int avg_value, min, max, count;
  bool is_combined_channels = false;
  uint16_t frame_size = 0;
  int result_factor = 1;
  int decim = 1;


  bool setupTx() {
    if (config.rx_tx_mode == TX_MODE) {
      // check channels
      if (config.channels > ANALOG_MAX_OUT_CHANNELS) {
        if (config.channels == 2) {
          is_combined_channels = true;
          config.channels = 1;
        } else {
          LOGE("Unsupported channels");
          return false;
        }
      }
      if (isDecimateActive()) {
        LOGI("Using reduced sample rate: %d", effectiveOutputSampleRate());
        decim = decimation();
        result_factor = result_factor * decim;
      }
      if (isCombinedChannel()) {
        LOGI("Combining channels");
        result_factor = result_factor * 2;
      }
    }
    return true;
  }

  bool setupBuffer() {
    if (buffer == nullptr) {
      // allocate buffer_count
      buffer = new RingBuffer<uint8_t>(config.buffer_size * config.buffer_count);
      if (buffer == nullptr) {
        LOGE("Not enough memory for buffer");
        return false;
      }
    }
    return true;
  }

  bool setupTimer() {
    int sample_rate = config.rx_tx_mode == TX_MODE ? effectiveOutputSampleRate()
                                                   : config.sample_rate;
    LOGI("sample_rate: %d", sample_rate);
    timer.setCallbackParameter(this);
    return timer.begin(callback, sample_rate, TimeUnit::HZ);
  }

  /// Sample data and write to buffer
  static void callback(void *arg) {
    int16_t value = 0;
    AnalogAudioArduino *self = (AnalogAudioArduino *)arg;
    // prevent NPE
    if (self->buffer == nullptr) return;

    // Logic for reading audio data
    if (self->config.rx_tx_mode == RX_MODE) {
      int channels = self->config.channels;
      for (int j = 0; j < channels; j++) {
        // provides value in range 0…4095
        value = analogRead(self->config.pins_data[j]);
        if (self->config.is_auto_center_read) {
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
        int pin = self->config.pins_data[j];
        analogWrite(pin, sample);
        //LOGW("analogWrite(%d, %d)", pin, sample);
      }
    }
  }

  /// pinmode input for defined analog pins
  bool setupPins() {
    TRACED();

    Pins& pins = config.pins();
    if (pins.size()<config.channels){
      LOGE("Only pins %d of %d defined", pins.size(), config.channels);
      return false;
    }


    if (config.rx_tx_mode == RX_MODE) {
      LOGI("rx start_pin: %d", config.start_pin);
      // setup pins for read
      for (int j = 0; j < config.channels; j++) {
        int pin = config.pins_data [j];
        pinMode(pin, INPUT);
        LOGD("pinMode(%d, INPUT)", pin);
      }

      if (config.is_auto_center_read) {
        // calculate the avarage value to center the signal
        for (int j = 0; j < 1024; j++) {
          updateMinMax(analogRead(config.pins_data[0]));
        }
        LOGI("Avg Signal was %d", avg_value);
      }
    } else if (config.rx_tx_mode == TX_MODE) {
      // setup pins for read
      for (int j = 0; j < config.channels; j++) {
        int pin = config.pins_data[j];
        LOGI("tx pin %d: %d", j, pin);
        pinMode(pin, OUTPUT);
        LOGD("pinMode(%d, OUTPUT)", pin);
      }
    }
    return true;
  }

  void updateMinMax(int value) {
    if (value < min) min = value;
    if (value > max) max = value;
    if (count++ == 1024) updateAvg();
  }

  void updateAvg() {
    avg_value = (max + min) / 2;
    min = INT_MAX;
    max = INT_MIN;
    count = 0;
  }

  /// The requested sampling rate is too hight: we only process half of the
  /// samples so we can half the sampling rate
  bool isDecimateActive() {
    return config.sample_rate >= config.max_sample_rate;
  }

  // combined stereo channel to mono
  bool isCombinedChannel() { return is_combined_channels; }

  /// Returns the effective output sample rate
  int effectiveOutputSampleRate() { return config.sample_rate / decimation(); }

  int decimation() {
    if (config.sample_rate <= config.max_sample_rate) return 1;
    for (int j = 2; j < 6; j += 2) {
      if (config.sample_rate / j <= config.max_sample_rate) {
        return j;
      }
    }
    return 6;
  }
};

}  // namespace audio_tools
