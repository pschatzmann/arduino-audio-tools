#pragma once
#include "AudioConfig.h"

#define INT24_MAX 0x7FFFFF

namespace audio_tools {

/**
 * @brief 24bit integer which is used for I2S sound processing. The values are represented as int32_t, but only 3 bytes are used.
 * If you assign values which are too big, they are clipped. 
 * @ingroup basic
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class int24_4bytes_t  {
 public:
  int24_4bytes_t() {
    value = 0;
  }

  int24_4bytes_t(void *ptr) {
    memcpy(&value, ptr, 4);
  }

  int24_4bytes_t(const int16_t in) {
    set(in) ;
  }

  int24_4bytes_t(const int32_t in) {
    set(in);
  }

  int24_4bytes_t(const int64_t in) {
    set((int32_t)in) ;
  }

  int24_4bytes_t(const int24_4bytes_t &in) {
    value = in.value;
  }

  int24_4bytes_t(const float in) {
    set((int32_t)in);
  }

#if defined(USE_INT24_FROM_INT) 

  int24_4bytes_t(const int in) {
    set(in);
  }

#endif

  /// values are clipped and shifted by 1 byte
  inline void set(const int32_t &in) {
    if (in>INT24_MAX){
      value = INT24_MAX << 8;
    } else if (in<-INT24_MAX){
      value = -(INT24_MAX << 8);
    } else {
      value = in << 8;
    }
  }

  int24_4bytes_t& operator=(const int24_4bytes_t& other){
    value = other.value;
    return *this;
  }

  int24_4bytes_t& operator=(const float& other){
    set(((int32_t)other));
    return *this;
  }

  operator int()  {
    return toInt();
  }  

  explicit operator float()  {
    return toInt();
  }  

  explicit operator int64_t()   {
    return toInt();
  }  

  int24_4bytes_t& operator +=(int32_t valueA){
    int32_t temp = toInt();
    temp += valueA;
    set(temp);
    return *this;
  }

  int24_4bytes_t& operator -=(int32_t valueA){
    int32_t temp = toInt();
    temp -= valueA;
    set(temp);
    return *this;
  }

  /// Standard Conversion to Int 
  int toInt() const {
    return value >> 8;
  }  

  /// convert to float
  float toFloat() const { return (float)toInt(); }

  /// provides value between -32767 and 32767
  int16_t scale16() const {
    return static_cast<int16_t>(toInt() >> 8) ;
  }

  /// provides value between -2,147,483,647 and 2,147,483,647
  int32_t scale32() const {
    return static_cast<int32_t>(toInt() << 8);
  }

  /// provides value between -1.0 and 1.0
  float scaleFloat() const { return toFloat() / INT24_MAX; }

  void setAndScale16(int16_t i16) {
    value = i16;
    value = value << 16;
  }
  
  int16_t getAndScale16() {
    return static_cast<int16_t>(value >> 16);
  }

 private:
  // store 24 bit value shifted by 1 byte to the left
  int32_t value;
};


}  // namespace audio_tools