#pragma once

#include "AudioConfig.h"
#include "AudioTimer/AudioTimer.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioOutput.h"
#include "AudioTools/Buffers.h"
#include "vector"

namespace audio_tools {


/**
 * @brief R2R configuration
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class R2RConfig : public AudioInfo {
 public:
  std::vector<int> channel1_pins;
  std::vector<int> channel2_pins;
  uint16_t buffer_size = DEFAULT_BUFFER_SIZE; // automatic determination from write size
  uint16_t buffer_count = 2; // double buffer
};

/**
 * @brief DRAFT implementation for Output to R2R DAC
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class R2ROutput : public AudioOutput {
 public:
  R2RConfig defaultConfig() {
    R2RConfig r;
    return r;
  }

  bool begin(R2RConfig c) {
    TRACED();
    cfg = c;
    rcfg = c;
    return begin();
  }

  bool begin() override {
    TRACED();
    if (cfg.channels == 0 || cfg.channels > 2) {
      LOGE("channels is %d", cfg.channels);
      return false;
    }
    if (rcfg.channel1_pins.size() == 0) {
      LOGE("channel1_pins not defined");
      return false;
    }
    if (cfg.channels == 2 &&
        rcfg.channel2_pins.size() != rcfg.channel1_pins.size()) {
      LOGE("channel2_pins not defined");
      return false;
    }
    if (rcfg.buffer_size * rcfg.buffer_count == 0){
      LOGE("buffer_size or buffer_count is 0");
      return false;
    }
    buffer.resize(rcfg.buffer_size, rcfg.buffer_count);
    setupPins();
    timer.setCallbackParameter(this);
    timer.setIsSave(true);
    return timer.begin(r2r_timer_callback, cfg.sample_rate, HZ);
  }

  size_t write(const uint8_t *data, size_t len) override {
    // if buffer has not been allocated (buffer_size==0)
    if (len > rcfg.buffer_size) {
      LOGE("buffer_size %d too small for write size: %d", rcfg.buffer_size, len);
      return len;
    }
    size_t result = buffer.writeArray(data, len);
    // activate output when buffer is half full
    if (!is_active && buffer.bufferCountFilled() >= rcfg.buffer_count / 2) {
      LOGI("is_active = true");
      is_active = true;
    }
    return result;
  }

 protected:
  TimerAlarmRepeating timer;
  // Double buffer
  NBuffer<uint8_t> buffer{DEFAULT_BUFFER_SIZE, 0};
  R2RConfig rcfg;

  virtual void setupPins() {
    TRACED();
    for (auto pin : rcfg.channel1_pins) {
      LOGI("Setup channel1 pin %d", pin);
      pinMode(pin, OUTPUT);
    }
    for (int pin : rcfg.channel2_pins) {
      LOGI("Setup channel2 pin %d", pin);
      pinMode(pin, OUTPUT);
    }
  }

  void writeValue(int channel) {
    switch (cfg.bits_per_sample) {
      case 8:
        return writeValueT<int8_t>(channel);
      case 16:
        return writeValueT<int16_t>(channel);
      case 24:
        return writeValueT<int24_t>(channel);
      case 32:
        return writeValueT<int32_t>(channel);
    }
  }

  template <typename T>
  void writeValueT(int channel) {
    // don't do anything if we do not have enough data
    if (buffer.available() < sizeof(T)) return;

    // get next value from buffer
    T value = 0;
    buffer.readArray((uint8_t *)&value, sizeof(T));
    // convert to unsigned
    unsigned uvalue = (int)value + NumberConverter::maxValueT<T>() + 1;
    // scale value
    uvalue = uvalue >> ((sizeof(T) * 8) - rcfg.channel1_pins.size());
    // Serial.println(uvalue);

    // output pins
    writePins(channel, uvalue);
  }

  virtual void writePins(int channel, unsigned uvalue){
    switch (channel) {
      case 0:
        for (int j = 0; j < rcfg.channel1_pins.size(); j++) {
          int pin = rcfg.channel1_pins[j];
          if (pin >= 0) digitalWrite(pin, (uvalue >> j) & 1);
        }
        break;
      case 1:
        for (int j = 0; j < rcfg.channel2_pins.size(); j++) {
          int pin = rcfg.channel2_pins[j];
          if (pin >= 0) digitalWrite(pin, (uvalue >> j) & 1);
        }
        break;
    }
  }

  static void r2r_timer_callback(void *ptr) {
    R2ROutput *self = (R2ROutput *)ptr;
    if (self->is_active) {
      // output channel 1
      self->writeValue(0);
      // output channel 2
      if (self->cfg.channels == 2) self->writeValue(1);
    };
  }
};

}  // namespace audio_tools