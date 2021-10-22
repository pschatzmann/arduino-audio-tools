#pragma once
#include "AudioConfig.h"

#define INT24_MAX 0x7FFFFF

namespace audio_tools {

/**
 * @brief 24bit integer which is used for I2S sound processing. It works only on little endian machines!
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class int24_t : public Printable {

  public:

    int24_t(){
        value[0]=0;
        value[1]=0;
        value[2]=0;
    }

    int24_t(void *ptr){
        uint8_t *ptrChar = static_cast<uint8_t*>(ptr);
        value[0]=ptrChar[0];
        value[1]=ptrChar[1];
        value[2]=ptrChar[2];
    }

    int24_t(uint8_t *ptr){
        value[0]=ptr[0];
        value[1]=ptr[1];
        value[2]=ptr[2];
    }

    int24_t(const int16_t &in) {
      value[2] = in > 0 ? 0 : 0xFF;
      value[1] = (in >> 8) & 0xFF;
      value[0] = in & 0xFF;
    }

    int24_t(const int32_t &in){
      value[2] = (in >> 16) & 0xFF;;
      value[1] = (in >> 8) & 0xFF;
      value[0] = in & 0xFF;
    }

    operator int32_t() const {
        uint8_t tmp[4];
        tmp[0] = value[0];
        tmp[1] = value[1];
        tmp[2] = value[2];
        tmp[3] = value[2] & 0x80  ? 0xFF : 0;
        int32_t *ptr = (int32_t *)&tmp;
        return *ptr;
    }

    operator float() const {
        return static_cast<float>(*this);
    }

    /// provides value between -32767 and 32767
    int16_t scale16() const {
        return static_cast<float>(*this) * INT16_MAX / INT24_MAX ; 
    }

    /// provides value between -2,147,483,647 and 2,147,483,647
    int32_t scale32() const {
        return static_cast<float>(*this) / static_cast<float>(INT24_MAX) * static_cast<float>(INT32_MAX) ; 
    }

    /// provides value between -1.0 and 1.0
    float scaleFloat() const {
        return static_cast<float>(*this)  / INT24_MAX ; 
    }

    virtual size_t printTo(Print& p) const {
        // just print as int32_t
        const int32_t v = *this;
        return p.print(v);
    }

  private:
    uint8_t value[3]; 
};

}