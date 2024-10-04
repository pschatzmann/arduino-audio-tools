#pragma once
#include "AudioConfig.h"

namespace audio_tools {

/**
 * @brief Stores float values as uint32_t so that we can use memory allocated with MALLOC_CAP_32BIT
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class float32 {
  public:
    float32() = default;
    float32(float value){
        this->value = float32::as_uint(value);
    }
    float32(const float32 &value){
        this->value = value.value;
    }
    inline operator float()  {
        return as_float(value);
    }
    explicit inline operator double()  {
        return (double) float32::as_float(value);
    }
    explicit inline operator int()  {
        return (int) float32::as_float(value);
    }
    explicit inline operator int()  const {
         return (int) float32::as_float(value);
    }
    inline bool operator<  (float32 other) const{
        return float() < (float)other;
    }
    inline bool operator<=  (float32 other) const{
        return float() <= (float)other;
    }
    inline bool operator>  (float32 other) const{
        return float() > (float)other;
    }
    inline bool operator>=  (float32 other) const{
        return float() >= (float)other;
    }
    inline bool operator==  (float32 other) const{
        return float() == (float)other;
    }
    inline bool operator!=  (float32 other) const{
        return float() != (float)other;
    }

  protected:
    uint32_t value=0;

    static uint32_t as_uint(float x) {
        return *(uint32_t*)&x;
    }
    static float as_float(uint32_t x) {
        return *(float*)&x;
    }

};

inline float operator+ (float32 one, float32 two) 
{
	return (float)one + (float)two;
}
inline float operator- (float32 one, float32 two) 
{
	return (float)one - (float)two;
}
inline float operator* (float32 one, float32 two) 
{
	return (float)one * (float)two;
}
inline float operator/ (float32 one, float32 two)
{
	return (float)one / (float)two;
}
inline float operator+ (float32 one, float two)
{
	return (float)one + two;
}
inline float operator- (float32 one, float two)
{
	return (float)one - two;
}
inline float operator* (float32 one, float two)
{
	return (float)one * two;
}
inline float operator/ (float32 one, float two)
{
	return (float)one / two;
}
inline float operator+ (float one, float32 two)
{
	return two + float(one);
}
inline float operator- (float one, float32 two)
{
	return two - float(one);
}
inline float operator* (float one, float32 two)
{
	return two * float(one);
}
inline float operator/ (float one, float32 two)
{
	return two / float(one);
}

}

namespace std {

inline float floor ( float32 arg ) { return std::floor((float)arg);}
inline float fabs ( float32 arg ) { return std::fabs((float)arg);}

}
