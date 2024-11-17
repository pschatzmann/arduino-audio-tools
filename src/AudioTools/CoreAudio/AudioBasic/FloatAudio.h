#pragma once
#include "AudioConfig.h"

namespace audio_tools {

/***
 * A simple float number (in the range of -1.0 to 1.0) that supports the conversion to 
 * it's corresponding scaled int values.
 */

class FloatAudio {
 public:
  FloatAudio() = default;
  FloatAudio(float in) { this->value = in; }

  inline operator float() { return value; }

  explicit inline operator int8_t() { return value * 127; }

  explicit inline operator int16_t() { return value * 32767; }

  explicit inline operator int24_3bytes_t() {
    return value * 8388607;
  }

  explicit inline operator int24_4bytes_t() {
    return value * 8388607;
  }

  explicit inline operator int32_t() { return value * 2147483647; }

 protected:
  float value = 0.0f;
};

}  // namespace audio_tools

#ifdef USE_TYPETRAITS

namespace std {
template <>
class numeric_limits<audio_tools::FloatAudio> {
 public:
  static audio_tools::FloatAudio lowest() {
    return audio_tools::FloatAudio(-1.0f);
  };
  static audio_tools::FloatAudio min() {
    return audio_tools::FloatAudio(-1.0f);
  };
  static audio_tools::FloatAudio max() {
    return audio_tools::FloatAudio(1.0f);
  };
};
}  // namespace std

#endif
