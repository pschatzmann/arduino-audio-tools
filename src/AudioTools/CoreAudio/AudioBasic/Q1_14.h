#pragma once
#include <stdint.h>
#include "AudioTools/CoreAudio/AudioBasic/int24_t.h"

namespace audio_tools {

/**
 * @brief Fixed-point Q1.14 number: a plain 16-bit signed integer holding 1
 * integer bit and 14 fractional bits (value = raw / 16384.0). Covers the
 * range [-2.0, 1.99993896484375] with a resolution of ~6.1e-5, using only
 * integer add/sub/mul/div -- no hardware floating-point instructions.
 * @ingroup basic
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class q1_14_t {
 public:
  static constexpr int kFractionalBits = 14;
  static constexpr int32_t kScale = 1 << kFractionalBits;  // 16384

  q1_14_t() = default;
  q1_14_t(const q1_14_t &in) = default;
  q1_14_t(float in) { value = fromFloat(in); }
  q1_14_t(double in) { value = fromFloat((float)in); }
  q1_14_t(int in) {
    value = (int16_t)clampRange64((int64_t)in * kScale, -32768, 32767);
  }

  q1_14_t &operator=(const q1_14_t &other) = default;
  q1_14_t &operator=(float other) {
    value = fromFloat(other);
    return *this;
  }

  operator float() const { return toFloat(); }
  /// truncates toward zero, like (int)someFloat -- integer-only, no FPU
  explicit operator int() const {
    int32_t iv = value;
    return iv < 0 ? -((-iv) >> kFractionalBits) : (iv >> kFractionalBits);
  }

  /// raw underlying Q1.14 representation
  int16_t raw() const { return value; }
  /// builds a q1_14_t directly from a raw Q1.14 value (no scaling)
  static q1_14_t fromRaw(int16_t raw) {
    q1_14_t result;
    result.value = raw;
    return result;
  }

  // PCM samples are treated as fixed-point with (bit_depth - 1) fractional
  // bits (Q1.15 for 16-bit, Q1.23 for 24-bit, Q1.31 for 32-bit), so
  // converting to/from Q1.14 (14 fractional bits) is just a left/right shift
  // by the difference in fractional bits -- integer-only, no FPU needed.

  /// converts to a 16-bit PCM sample (-32768..32767)
  int16_t toInt16() const {
    return (int16_t)clampRange32((int32_t)value << 1, -32768, 32767);
  }
  /// converts to a 24-bit PCM sample (-8388608..8388607)
  int24_t toInt24() const {
    return clampRange32((int32_t)value << 9, -8388608, 8388607);
  }
  /// converts to a 32-bit PCM sample (-2147483648..2147483647)
  int32_t toInt32() const {
    return (int32_t)clampRange64((int64_t)value << 17, INT32_MIN, INT32_MAX);
  }

  /// builds a q1_14_t from a 16-bit PCM sample (-32768..32767)
  static q1_14_t fromInt16(int16_t sample) { return fromRaw((int16_t)(sample >> 1)); }
  /// builds a q1_14_t from a 24-bit PCM sample (-8388608..8388607)
  static q1_14_t fromInt24(int24_t sample) {
    return fromRaw((int16_t)(((int32_t)sample) >> 9));
  }
  /// builds a q1_14_t from a 32-bit PCM sample (-2147483648..2147483647)
  static q1_14_t fromInt32(int32_t sample) { return fromRaw((int16_t)(sample >> 17)); }

  // Applies this Q1.14 value as a gain/volume factor to a full-scale PCM
  // sample (sample * factor), clipping on overflow -- integer-only, no FPU
  // needed. Unlike operator*, the sample here is NOT itself Q1.14-scaled.

  /// scales a 16-bit PCM sample by this Q1.14 factor
  int16_t scale(int16_t sample) const {
    int32_t prod = ((int32_t)sample * value) >> kFractionalBits;
    return (int16_t)clampRange32(prod, -32768, 32767);
  }
  /// scales a 24-bit PCM sample by this Q1.14 factor
  int24_t scale(int24_t sample) const {
    int64_t prod = ((int64_t)(int32_t)sample * value) >> kFractionalBits;
    return (int32_t)clampRange64(prod, -8388608, 8388607);
  }
  /// scales a 32-bit PCM sample by this Q1.14 factor
  int32_t scale(int32_t sample) const {
    int64_t prod = ((int64_t)sample * value) >> kFractionalBits;
    return (int32_t)clampRange64(prod, INT32_MIN, INT32_MAX);
  }

  q1_14_t operator+(q1_14_t o) const {
    return fromRaw(clamp((int32_t)value + o.value));
  }
  q1_14_t operator-(q1_14_t o) const {
    return fromRaw(clamp((int32_t)value - o.value));
  }
  q1_14_t operator-() const { return fromRaw(clamp(-(int32_t)value)); }

  q1_14_t operator*(q1_14_t o) const {
    int32_t prod = ((int32_t)value * (int32_t)o.value) >> kFractionalBits;
    return fromRaw(clamp(prod));
  }

  q1_14_t operator/(q1_14_t o) const {
    if (o.value == 0) {
      // saturate instead of dividing by zero
      return fromRaw(value < 0 ? INT16_MIN : INT16_MAX);
    }
    int32_t quotient = (((int32_t)value) << kFractionalBits) / o.value;
    return fromRaw(clamp(quotient));
  }

  q1_14_t &operator+=(q1_14_t o) { return *this = *this + o; }
  q1_14_t &operator-=(q1_14_t o) { return *this = *this - o; }
  q1_14_t &operator*=(q1_14_t o) { return *this = *this * o; }
  q1_14_t &operator/=(q1_14_t o) { return *this = *this / o; }

  bool operator<(q1_14_t o) const { return value < o.value; }
  bool operator<=(q1_14_t o) const { return value <= o.value; }
  bool operator>(q1_14_t o) const { return value > o.value; }
  bool operator>=(q1_14_t o) const { return value >= o.value; }
  bool operator==(q1_14_t o) const { return value == o.value; }
  bool operator!=(q1_14_t o) const { return value != o.value; }

  // q1_14_t/float mixed overloads: q1_14_t has both a converting
  // constructor from float and a conversion operator to float, so
  // `q1_14Value * 0.7f` would otherwise be ambiguous between converting the
  // float to q1_14_t or the q1_14_t to float. These exact-match overloads
  // resolve that.
  q1_14_t operator+(float o) const { return *this + q1_14_t(o); }
  q1_14_t operator-(float o) const { return *this - q1_14_t(o); }
  q1_14_t operator*(float o) const { return *this * q1_14_t(o); }
  q1_14_t operator/(float o) const { return *this / q1_14_t(o); }
  bool operator<(float o) const { return *this < q1_14_t(o); }
  bool operator<=(float o) const { return *this <= q1_14_t(o); }
  bool operator>(float o) const { return *this > q1_14_t(o); }
  bool operator>=(float o) const { return *this >= q1_14_t(o); }
  bool operator==(float o) const { return *this == q1_14_t(o); }
  bool operator!=(float o) const { return *this != q1_14_t(o); }

 protected:
  int16_t value = 0;

  float toFloat() const { return (float)value / (float)kScale; }

  static int16_t fromFloat(float in) {
    float scaled = in * (float)kScale;
    scaled += (scaled >= 0.0f ? 0.5f : -0.5f);  // round to nearest
    if (scaled >= 32767.0f) return 32767;
    if (scaled <= -32768.0f) return -32768;
    return (int16_t)scaled;
  }

  static int16_t clamp(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
  }

  static int32_t clampRange32(int32_t v, int32_t lo, int32_t hi) {
    if (v > hi) return hi;
    if (v < lo) return lo;
    return v;
  }

  static int64_t clampRange64(int64_t v, int64_t lo, int64_t hi) {
    if (v > hi) return hi;
    if (v < lo) return lo;
    return v;
  }
};

// Free-function overloads for `float op q1_14_t` (float on the left), for
// the same exact-match-ambiguity reason as the member float overloads above.
inline q1_14_t operator+(float a, q1_14_t b) { return q1_14_t(a) + b; }
inline q1_14_t operator-(float a, q1_14_t b) { return q1_14_t(a) - b; }
inline q1_14_t operator*(float a, q1_14_t b) { return q1_14_t(a) * b; }
inline q1_14_t operator/(float a, q1_14_t b) { return q1_14_t(a) / b; }
inline bool operator<(float a, q1_14_t b) { return q1_14_t(a) < b; }
inline bool operator<=(float a, q1_14_t b) { return q1_14_t(a) <= b; }
inline bool operator>(float a, q1_14_t b) { return q1_14_t(a) > b; }
inline bool operator>=(float a, q1_14_t b) { return q1_14_t(a) >= b; }
inline bool operator==(float a, q1_14_t b) { return q1_14_t(a) == b; }
inline bool operator!=(float a, q1_14_t b) { return q1_14_t(a) != b; }

}  // namespace audio_tools
