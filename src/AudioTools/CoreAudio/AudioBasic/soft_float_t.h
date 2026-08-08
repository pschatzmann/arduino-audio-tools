#pragma once

#include <cmath>
#include <cstdint>
#include "AudioTools/CoreAudio/AudioBasic/int24_t.h"

namespace audio_tools {

/**
 * @brief Software "pseudo-float": a real number represented as a signed
 * 16-bit mantissa plus a 16-bit exponent (value = mantissa * 2^exponent),
 * computed with integer multiply/shift/compare only -- no hardware
 * floating-point instructions. This generalizes speexdsp's classic
 * fixed-point trick of tracking a handful of wide-dynamic-range control
 * variables as a normalized mantissa+exponent pair into a full,
 * operator-overloaded numeric type usable as a drop-in replacement for
 * `float` (see MDFFixedPoint in MDFEchoCancellation.h, which uses this
 * class as the element type for the MDF echo canceller's sample/spectrum
 * arrays and internal state, so the whole algorithm runs without native
 * float arithmetic).
 *
 * Unlike a plain fixed Q-format (a fixed number of fractional bits, which
 * only covers a fixed range/precision trade-off and would over/underflow
 * badly across this algorithm's actual value range -- audio samples, FFT
 * magnitudes in the thousands, and adaptation-rate fractions near zero, all
 * flowing through the same arrays), this type renormalizes on every
 * operation, so it tolerates the same wide dynamic range as `float`, at
 * roughly the precision of a 16-bit mantissa (a relative error on the order
 * of 2^-15, i.e. about 4-5 significant decimal digits -- comparable to a
 * compressed/half-precision float).
 *
 * @note Comparison operators (`<`, `>`, etc.) convert both sides to `float`
 * and compare natively -- they're scalar, infrequent control-flow checks,
 * not the per-element array math this class exists to keep off the FPU, so
 * trading a little purity there for simpler, more obviously correct code is
 * a reasonable trade. Arithmetic that mixes a soft_float_t with a plain
 * float/double literal (e.g. `0.7f * pseudoFloatValue`) also still executes
 * that specific operation using native float arithmetic via the implicit
 * conversions below -- avoiding *all* native float instructions everywhere
 * would require rewriting every numeric literal in the algorithm as a
 * soft_float_t, which isn't done here. What this class does guarantee is
 * that soft_float_t-to-soft_float_t arithmetic -- the tight inner loops over
 * whole arrays (FFT bins, filter weights, correlations) that dominate the
 * algorithm's cost -- runs on integer mantissa/exponent math throughout.
 */
class soft_float_t {
 public:
  constexpr soft_float_t() = default;
  constexpr soft_float_t(int16_t mantissa, int16_t exponent)
      : m_(mantissa), e_(exponent) {}
  soft_float_t(float f) { fromFloat(f); }
  soft_float_t(double d) { fromFloat((float)d); }
  soft_float_t(int i) { fromFloat((float)i); }

  operator float() const { return toFloat(); }
  /// truncates toward zero, like (int)someFloat
  explicit operator int() const {
    float v = toFloat();
    if (v >= 2147483647.0f) return INT32_MAX;
    if (v <= -2147483648.0f) return INT32_MIN;
    return (int)v;
  }

  // PCM samples are interpreted at their raw magnitude (e.g. up to +-32767
  // for 16-bit audio) rather than normalized, matching the convention used
  // when T=float elsewhere in this codebase -- soft_float_t's wide dynamic
  // range (unlike q1_14_t's bounded +-2.0) means no rescaling is needed.

  /// converts to a 16-bit PCM sample (-32768..32767)
  int16_t toInt16() const { return (int16_t)clampRound(toFloat(), -32768.0, 32767.0); }
  /// converts to a 24-bit PCM sample (-8388608..8388607)
  int24_t toInt24() const { return clampRound(toFloat(), -8388608.0, 8388607.0); }
  /// converts to a 32-bit PCM sample (-2147483648..2147483647)
  int32_t toInt32() const { return clampRound(toFloat(), -2147483648.0, 2147483647.0); }

  /// builds a soft_float_t from a 16-bit PCM sample (-32768..32767)
  static soft_float_t fromInt16(int16_t sample) { return soft_float_t((float)sample); }
  /// builds a soft_float_t from a 24-bit PCM sample (-8388608..8388607)
  static soft_float_t fromInt24(int24_t sample) {
    return soft_float_t((float)(int32_t)sample);
  }
  /// builds a soft_float_t from a 32-bit PCM sample (-2147483648..2147483647)
  static soft_float_t fromInt32(int32_t sample) { return soft_float_t((float)sample); }

  // Applies this value as a gain/volume factor to a full-scale PCM sample
  // (sample * factor), clipping on overflow. Unlike operator*, the sample
  // here is NOT itself a soft_float_t-scaled value.

  /// scales a 16-bit PCM sample by this factor
  int16_t scale(int16_t sample) const {
    return (int16_t)clampRound(toFloat() * (double)sample, -32768.0, 32767.0);
  }
  /// scales a 24-bit PCM sample by this factor
  int24_t scale(int24_t sample) const {
    return clampRound(toFloat() * (double)(int32_t)sample, -8388608.0, 8388607.0);
  }
  /// scales a 32-bit PCM sample by this factor
  int32_t scale(int32_t sample) const {
    return clampRound(toFloat() * (double)sample, -2147483648.0, 2147483647.0);
  }

  soft_float_t operator+(soft_float_t o) const {
    return addAligned(m_, e_, o.m_, o.e_);
  }
  soft_float_t operator-(soft_float_t o) const { return *this + (-o); }
  soft_float_t operator-() const {
    return soft_float_t((int16_t)(-(int32_t)m_), e_);
  }

  soft_float_t operator*(soft_float_t o) const {
    int32_t prod = ((int32_t)m_ * (int32_t)o.m_) >> 15;
    return normalize(prod, (int32_t)e_ + o.e_ + 15);
  }

  soft_float_t operator/(soft_float_t o) const {
    if (o.m_ == 0) {
      // Match WORD2INT-style saturation elsewhere in this codebase: a very
      // large (but finite, sign-matching) result instead of a hardware trap.
      return soft_float_t((int16_t)(m_ < 0 ? -32767 : 32767), (int16_t)30);
    }
    int32_t numerator = (int32_t)m_ << 15;
    int32_t quotient = numerator / o.m_;
    return normalize(quotient, (int32_t)e_ - o.e_ - 15);
  }

  soft_float_t& operator+=(soft_float_t o) { return *this = *this + o; }
  soft_float_t& operator-=(soft_float_t o) { return *this = *this - o; }
  soft_float_t& operator*=(soft_float_t o) { return *this = *this * o; }
  soft_float_t& operator/=(soft_float_t o) { return *this = *this / o; }

  bool operator<(soft_float_t o) const { return toFloat() < o.toFloat(); }
  bool operator>(soft_float_t o) const { return toFloat() > o.toFloat(); }
  bool operator<=(soft_float_t o) const { return toFloat() <= o.toFloat(); }
  bool operator>=(soft_float_t o) const { return toFloat() >= o.toFloat(); }
  bool operator==(soft_float_t o) const { return m_ == o.m_ && e_ == o.e_; }
  bool operator!=(soft_float_t o) const { return !(*this == o); }

  // soft_float_t has both a converting constructor from float and a
  // conversion operator to float, so `pseudoFloatValue * 0.7f` (and every
  // other mixed op) would otherwise be ambiguous -- the compiler can't
  // choose between converting the float to soft_float_t (to use the member
  // operator above) or converting the soft_float_t to float (to use the
  // built-in operator), and both need exactly one user-defined conversion.
  // These exact-match overloads (no conversion needed for either operand)
  // resolve that ambiguity, since the algorithm this class backs mixes
  // float literals and soft_float_t variables constantly.
  soft_float_t operator+(float o) const { return *this + soft_float_t(o); }
  soft_float_t operator-(float o) const { return *this - soft_float_t(o); }
  soft_float_t operator*(float o) const { return *this * soft_float_t(o); }
  soft_float_t operator/(float o) const { return *this / soft_float_t(o); }
  bool operator<(float o) const { return *this < soft_float_t(o); }
  bool operator>(float o) const { return *this > soft_float_t(o); }
  bool operator<=(float o) const { return *this <= soft_float_t(o); }
  bool operator>=(float o) const { return *this >= soft_float_t(o); }
  bool operator==(float o) const { return *this == soft_float_t(o); }
  bool operator!=(float o) const { return *this != soft_float_t(o); }

  // Same exact-match-ambiguity issue as the float overloads above, but for
  // `int` literals (e.g. `1 - pseudoFloatValue`, `radius > 16383`): without
  // these, the compiler has to choose between the float overload (needs an
  // int->float conversion) and the built-in int operator (reachable from
  // soft_float_t via the implicit operator float() followed by a
  // floating-integral conversion), and neither dominates the other.
  soft_float_t operator+(int o) const { return *this + soft_float_t(o); }
  soft_float_t operator-(int o) const { return *this - soft_float_t(o); }
  soft_float_t operator*(int o) const { return *this * soft_float_t(o); }
  soft_float_t operator/(int o) const { return *this / soft_float_t(o); }
  bool operator<(int o) const { return *this < soft_float_t(o); }
  bool operator>(int o) const { return *this > soft_float_t(o); }
  bool operator<=(int o) const { return *this <= soft_float_t(o); }
  bool operator>=(int o) const { return *this >= soft_float_t(o); }
  bool operator==(int o) const { return *this == soft_float_t(o); }
  bool operator!=(int o) const { return *this != soft_float_t(o); }

  // Same exact-match-ambiguity issue again, but for `double` literals --
  // e.g. unsuffixed literals like `1.` or `.5`, or macros such as the
  // Arduino core's `PI` (ArduinoCore-API/api/Common.h defines it as an
  // unsuffixed double, so it stays double even on real hardware, not just
  // desktop builds).
  soft_float_t operator+(double o) const { return *this + soft_float_t(o); }
  soft_float_t operator-(double o) const { return *this - soft_float_t(o); }
  soft_float_t operator*(double o) const { return *this * soft_float_t(o); }
  soft_float_t operator/(double o) const { return *this / soft_float_t(o); }
  bool operator<(double o) const { return *this < soft_float_t(o); }
  bool operator>(double o) const { return *this > soft_float_t(o); }
  bool operator<=(double o) const { return *this <= soft_float_t(o); }
  bool operator>=(double o) const { return *this >= soft_float_t(o); }
  bool operator==(double o) const { return *this == soft_float_t(o); }
  bool operator!=(double o) const { return *this != soft_float_t(o); }

  int16_t mantissa() const { return m_; }
  int16_t exponent() const { return e_; }

 private:
  int16_t m_ = 0;  // normalized magnitude in [16384, 32767] (0 iff value == 0)
  int16_t e_ = 0;  // value = m_ * 2^e_

  /// rounds to nearest and clamps to [lo, hi], for the PCM boundary
  /// conversions (toIntNN/scale) above.
  static int32_t clampRound(double v, double lo, double hi) {
    if (v >= hi) return (int32_t)hi;
    if (v <= lo) return (int32_t)lo;
    v += (v >= 0.0 ? 0.5 : -0.5);
    return (int32_t)v;
  }

  void fromFloat(float f) {
    if (f == 0.0f) {
      m_ = 0;
      e_ = 0;
      return;
    }
    bool neg = f < 0;
    double mag = neg ? -(double)f : (double)f;
    int32_t e = 0;
    while (mag >= 32768.0) {
      mag *= 0.5;
      e++;
    }
    while (mag < 16384.0) {
      mag *= 2.0;
      e--;
    }
    int16_t mag16 = (int16_t)mag;
    m_ = neg ? (int16_t)(-(int32_t)mag16) : mag16;
    e_ = (int16_t)e;
  }

  float toFloat() const {
    return (float)((double)m_ * pow(2.0, (double)e_));
  }

  /// Renormalizes an arbitrary-magnitude signed value + exponent back into
  /// the class's [16384, 32767]-magnitude mantissa range.
  static soft_float_t normalize(int32_t val, int32_t e) {
    if (val == 0) return soft_float_t((int16_t)0, (int16_t)0);
    bool neg = val < 0;
    int64_t mag = neg ? -(int64_t)val : (int64_t)val;
    while (mag > 32767) {
      mag >>= 1;
      e++;
    }
    while (mag < 16384) {
      mag <<= 1;
      e--;
    }
    int16_t mi = (int16_t)(neg ? -mag : mag);
    return soft_float_t(mi, (int16_t)e);
  }

  /// Adds two (mantissa, exponent) pairs by aligning the smaller exponent
  /// up to the larger one before summing, then renormalizing the result.
  static soft_float_t addAligned(int16_t ma, int16_t ea, int16_t mb,
                                 int16_t eb) {
    if (ma == 0) return soft_float_t(mb, eb);
    if (mb == 0) return soft_float_t(ma, ea);
    int32_t a = ma, b = mb;
    int32_t e;
    if (ea >= eb) {
      int shift = ea - eb;
      b = (shift >= 31) ? 0 : (b >> shift);
      e = ea;
    } else {
      int shift = eb - ea;
      a = (shift >= 31) ? 0 : (a >> shift);
      e = eb;
    }
    return normalize(a + b, e);
  }
};

// Free-function overloads for `float op soft_float_t` (float on the left),
// for the same exact-match-ambiguity reason as the member float overloads
// above.
inline soft_float_t operator+(float a, soft_float_t b) { return soft_float_t(a) + b; }
inline soft_float_t operator-(float a, soft_float_t b) { return soft_float_t(a) - b; }
inline soft_float_t operator*(float a, soft_float_t b) { return soft_float_t(a) * b; }
inline soft_float_t operator/(float a, soft_float_t b) { return soft_float_t(a) / b; }
inline bool operator<(float a, soft_float_t b) { return soft_float_t(a) < b; }
inline bool operator>(float a, soft_float_t b) { return soft_float_t(a) > b; }
inline bool operator<=(float a, soft_float_t b) { return soft_float_t(a) <= b; }
inline bool operator>=(float a, soft_float_t b) { return soft_float_t(a) >= b; }
inline bool operator==(float a, soft_float_t b) { return soft_float_t(a) == b; }
inline bool operator!=(float a, soft_float_t b) { return soft_float_t(a) != b; }

// Free-function overloads for `int op soft_float_t` (int on the left), for
// the same reason as the free `float` overloads above.
inline soft_float_t operator+(int a, soft_float_t b) { return soft_float_t(a) + b; }
inline soft_float_t operator-(int a, soft_float_t b) { return soft_float_t(a) - b; }
inline soft_float_t operator*(int a, soft_float_t b) { return soft_float_t(a) * b; }
inline soft_float_t operator/(int a, soft_float_t b) { return soft_float_t(a) / b; }
inline bool operator<(int a, soft_float_t b) { return soft_float_t(a) < b; }
inline bool operator>(int a, soft_float_t b) { return soft_float_t(a) > b; }
inline bool operator<=(int a, soft_float_t b) { return soft_float_t(a) <= b; }
inline bool operator>=(int a, soft_float_t b) { return soft_float_t(a) >= b; }
inline bool operator==(int a, soft_float_t b) { return soft_float_t(a) == b; }
inline bool operator!=(int a, soft_float_t b) { return soft_float_t(a) != b; }

// Free-function overloads for `double op soft_float_t` (double on the
// left), for the same reason as the free `float`/`int` overloads above.
inline soft_float_t operator+(double a, soft_float_t b) { return soft_float_t(a) + b; }
inline soft_float_t operator-(double a, soft_float_t b) { return soft_float_t(a) - b; }
inline soft_float_t operator*(double a, soft_float_t b) { return soft_float_t(a) * b; }
inline soft_float_t operator/(double a, soft_float_t b) { return soft_float_t(a) / b; }
inline bool operator<(double a, soft_float_t b) { return soft_float_t(a) < b; }
inline bool operator>(double a, soft_float_t b) { return soft_float_t(a) > b; }
inline bool operator<=(double a, soft_float_t b) { return soft_float_t(a) <= b; }
inline bool operator>=(double a, soft_float_t b) { return soft_float_t(a) >= b; }
inline bool operator==(double a, soft_float_t b) { return soft_float_t(a) == b; }
inline bool operator!=(double a, soft_float_t b) { return soft_float_t(a) != b; }

/// @deprecated use soft_float_t
using PseudoFloat = soft_float_t;

}  // namespace audio_tools
