#pragma once

#include <cmath>
#include <cstdint>

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
 * a reasonable trade. Arithmetic that mixes a PseudoFloat with a plain
 * float/double literal (e.g. `0.7f * pseudoFloatValue`) also still executes
 * that specific operation using native float arithmetic via the implicit
 * conversions below -- avoiding *all* native float instructions everywhere
 * would require rewriting every numeric literal in the algorithm as a
 * PseudoFloat, which isn't done here. What this class does guarantee is
 * that PseudoFloat-to-PseudoFloat arithmetic -- the tight inner loops over
 * whole arrays (FFT bins, filter weights, correlations) that dominate the
 * algorithm's cost -- runs on integer mantissa/exponent math throughout.
 */
class PseudoFloat {
 public:
  constexpr PseudoFloat() = default;
  constexpr PseudoFloat(int16_t mantissa, int16_t exponent)
      : m_(mantissa), e_(exponent) {}
  PseudoFloat(float f) { fromFloat(f); }
  PseudoFloat(double d) { fromFloat((float)d); }
  PseudoFloat(int i) { fromFloat((float)i); }

  operator float() const { return toFloat(); }

  PseudoFloat operator+(PseudoFloat o) const {
    return addAligned(m_, e_, o.m_, o.e_);
  }
  PseudoFloat operator-(PseudoFloat o) const { return *this + (-o); }
  PseudoFloat operator-() const {
    return PseudoFloat((int16_t)(-(int32_t)m_), e_);
  }

  PseudoFloat operator*(PseudoFloat o) const {
    int32_t prod = ((int32_t)m_ * (int32_t)o.m_) >> 15;
    return normalize(prod, (int32_t)e_ + o.e_ + 15);
  }

  PseudoFloat operator/(PseudoFloat o) const {
    if (o.m_ == 0) {
      // Match WORD2INT-style saturation elsewhere in this codebase: a very
      // large (but finite, sign-matching) result instead of a hardware trap.
      return PseudoFloat((int16_t)(m_ < 0 ? -32767 : 32767), (int16_t)30);
    }
    int32_t numerator = (int32_t)m_ << 15;
    int32_t quotient = numerator / o.m_;
    return normalize(quotient, (int32_t)e_ - o.e_ - 15);
  }

  PseudoFloat& operator+=(PseudoFloat o) { return *this = *this + o; }
  PseudoFloat& operator-=(PseudoFloat o) { return *this = *this - o; }
  PseudoFloat& operator*=(PseudoFloat o) { return *this = *this * o; }
  PseudoFloat& operator/=(PseudoFloat o) { return *this = *this / o; }

  bool operator<(PseudoFloat o) const { return toFloat() < o.toFloat(); }
  bool operator>(PseudoFloat o) const { return toFloat() > o.toFloat(); }
  bool operator<=(PseudoFloat o) const { return toFloat() <= o.toFloat(); }
  bool operator>=(PseudoFloat o) const { return toFloat() >= o.toFloat(); }
  bool operator==(PseudoFloat o) const { return m_ == o.m_ && e_ == o.e_; }
  bool operator!=(PseudoFloat o) const { return !(*this == o); }

  // PseudoFloat has both a converting constructor from float and a
  // conversion operator to float, so `pseudoFloatValue * 0.7f` (and every
  // other mixed op) would otherwise be ambiguous -- the compiler can't
  // choose between converting the float to PseudoFloat (to use the member
  // operator above) or converting the PseudoFloat to float (to use the
  // built-in operator), and both need exactly one user-defined conversion.
  // These exact-match overloads (no conversion needed for either operand)
  // resolve that ambiguity, since the algorithm this class backs mixes
  // float literals and PseudoFloat variables constantly.
  PseudoFloat operator+(float o) const { return *this + PseudoFloat(o); }
  PseudoFloat operator-(float o) const { return *this - PseudoFloat(o); }
  PseudoFloat operator*(float o) const { return *this * PseudoFloat(o); }
  PseudoFloat operator/(float o) const { return *this / PseudoFloat(o); }
  bool operator<(float o) const { return *this < PseudoFloat(o); }
  bool operator>(float o) const { return *this > PseudoFloat(o); }
  bool operator<=(float o) const { return *this <= PseudoFloat(o); }
  bool operator>=(float o) const { return *this >= PseudoFloat(o); }
  bool operator==(float o) const { return *this == PseudoFloat(o); }
  bool operator!=(float o) const { return *this != PseudoFloat(o); }

  int16_t mantissa() const { return m_; }
  int16_t exponent() const { return e_; }

 private:
  int16_t m_ = 0;  // normalized magnitude in [16384, 32767] (0 iff value == 0)
  int16_t e_ = 0;  // value = m_ * 2^e_

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
  static PseudoFloat normalize(int32_t val, int32_t e) {
    if (val == 0) return PseudoFloat((int16_t)0, (int16_t)0);
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
    return PseudoFloat(mi, (int16_t)e);
  }

  /// Adds two (mantissa, exponent) pairs by aligning the smaller exponent
  /// up to the larger one before summing, then renormalizing the result.
  static PseudoFloat addAligned(int16_t ma, int16_t ea, int16_t mb,
                                 int16_t eb) {
    if (ma == 0) return PseudoFloat(mb, eb);
    if (mb == 0) return PseudoFloat(ma, ea);
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

// Free-function overloads for `float op PseudoFloat` (float on the left),
// for the same exact-match-ambiguity reason as the member float overloads
// above.
inline PseudoFloat operator+(float a, PseudoFloat b) { return PseudoFloat(a) + b; }
inline PseudoFloat operator-(float a, PseudoFloat b) { return PseudoFloat(a) - b; }
inline PseudoFloat operator*(float a, PseudoFloat b) { return PseudoFloat(a) * b; }
inline PseudoFloat operator/(float a, PseudoFloat b) { return PseudoFloat(a) / b; }
inline bool operator<(float a, PseudoFloat b) { return PseudoFloat(a) < b; }
inline bool operator>(float a, PseudoFloat b) { return PseudoFloat(a) > b; }
inline bool operator<=(float a, PseudoFloat b) { return PseudoFloat(a) <= b; }
inline bool operator>=(float a, PseudoFloat b) { return PseudoFloat(a) >= b; }
inline bool operator==(float a, PseudoFloat b) { return PseudoFloat(a) == b; }
inline bool operator!=(float a, PseudoFloat b) { return PseudoFloat(a) != b; }

}  // namespace audio_tools
