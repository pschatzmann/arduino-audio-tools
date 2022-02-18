#pragma once
#include "AudioConfig.h"

#define INT24_MAX 0x7FFFFF

namespace audio_tools {

/**
 * @brief 24bit integer which is used for I2S sound processing. It works only on
 * little endian machines!
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class int24_t  {
 public:
  int24_t() {
    value[0] = 0;
    value[1] = 0;
    value[2] = 0;
  }

  int24_t(void *ptr) {
      memcpy(value, ptr, 4);
  }

  int24_t(const int16_t &in) {
    value[2] = in > 0 ? 0 : 0xFF;
    value[1] = (in >> 8) & 0xFF;
    value[0] = in & 0xFF;
  }

  int24_t(const int32_t &in) {
    value[2] = (in >> 16) & 0xFF;
    value[1] = (in >> 8) & 0xFF;
    value[0] = in & 0xFF;
  }

  operator int() const {
    return toInt();
  }  

  /// Standard Conversion to Int
  int toInt() const {
    int newInt = (((0xFF & value[0]) << 16) | ((0xFF & value[1]) << 8) | (0xFF & value[2]));
    if ((newInt & 0x00800000) > 0) {
      newInt |= 0xFF000000;
    } else {
      newInt &= 0x00FFFFFF;
    }
    return newInt;
  }  

  /// convert to float
  float toFloat() const { return int(); }

  /// provides value between -32767 and 32767
  int16_t scale16() const {
    return toInt() * INT16_MAX / INT24_MAX;
  }

  /// provides value between -2,147,483,647 and 2,147,483,647
  int32_t scale32() const {
    return toInt()  *  (INT32_MAX / INT24_MAX); 
  }

  /// provides value between -1.0 and 1.0
  float scaleFloat() const { return toFloat() / INT24_MAX; }

 private:
  uint8_t value[3];
};


}  // namespace audio_tools