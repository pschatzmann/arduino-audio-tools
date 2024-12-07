#pragma once
#include "AudioConfig.h"

#define INT24_MAX 0x7FFFFF

namespace audio_tools {

/**
 * @brief 24bit integer which is used for I2S sound processing. The values are really using 3 bytes.
 * It works only on little endian machines!
 * @ingroup basic
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class int24_3bytes_t  {
 public:
  int24_3bytes_t() {
    value[0] = 0;
    value[1] = 0;
    value[2] = 0;
  }

  int24_3bytes_t(void *ptr) {
      memcpy(value, ptr, 3);
  }

  int24_3bytes_t(const int16_t &in) {
    value[2] = in > 0 ? 0 : 0xFF;
    value[1] = (in >> 8) & 0xFF;
    value[0] = in & 0xFF;
  }

  int24_3bytes_t(const int32_t &in) {
    set(in);
  }

  int24_3bytes_t(const int64_t &in) {
    set(in);
  }

  int24_3bytes_t(const float in) {
    set((int32_t)in);
  }

#if defined(USE_INT24_FROM_INT) 

  int24_3bytes_t(const int &in) {
    set(in);
  }

#endif

  void set(const int32_t &in) {
    value[2] = (in >> 16) & 0xFF;
    value[1] = (in >> 8) & 0xFF;
    value[0] = in & 0xFF;
  }

  int24_3bytes_t& operator=(const int24_3bytes_t& other){
    set(other);
    return *this;
  }

  int24_3bytes_t& operator=(const float& other){
    set((int32_t)other);
    return *this;
  }

  operator int() const {
    return toInt();
  }  

  int24_3bytes_t& operator +=(int32_t value){
    int32_t temp = toInt();
    temp += value;
    set(temp);
    return *this;
  }

  int24_3bytes_t& operator -=(int32_t value){
    int32_t temp = toInt();
    temp -= value;
    set(temp);
    return *this;
  }

  /// Standard Conversion to Int
  int toInt() const {
    int newInt = ((((int32_t)0xFF & value[0]) << 16) | (((int32_t)0xFF & value[1]) << 8) | ((int32_t)0xFF & value[2]));
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


  void setAndScale16(int16_t i16) {
    value[0] = 0;  // clear trailing byte
    int16_t *p16 = (int16_t *)&value[1];
    *p16 = i16;
  }
  int16_t getAndScale16() {
    int16_t *p16 = (int16_t *)&value[1];
    return *p16;
  }


 private:
  uint8_t value[3];
};


}  // namespace audio_tools

#ifdef USE_TYPETRAITS
#include <limits>

namespace std {
    template<> class numeric_limits<audio_tools::int24_3bytes_t> {
    public:
       static audio_tools::int24_3bytes_t lowest() {return audio_tools::int24_3bytes_t(-0x7FFFFF);};
       static audio_tools::int24_3bytes_t min() {return audio_tools::int24_3bytes_t(-0x7FFFFF);};
       static audio_tools::int24_3bytes_t max() {return audio_tools::int24_3bytes_t(0x7FFFFF);};
    };
}

#endif