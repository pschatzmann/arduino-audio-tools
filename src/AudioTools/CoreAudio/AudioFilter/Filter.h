#pragma once
#include <math.h>

#include "AudioToolsConfig.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
#ifdef USE_TYPETRAITS
#include <type_traits>
#endif


/**
 * @defgroup filter Filters
 * @ingroup dsp
 * @brief Digital Filters
 **/

namespace audio_tools {

/**
 * @brief Abstract filter interface definition. Subclasses implement process()
 * to transform audio samples one at a time. Use reset() to clear internal
 * state (e.g. delay lines) without changing the filter coefficients.
 * @ingroup filter
 * @author pschatzmann
 * @tparam T sample type (e.g. int16_t, float, double)
 */
template <typename T = float>
class Filter {
 public:
  // construct without coefs
  Filter() = default;
  virtual ~Filter() = default;
  Filter(Filter const&) = delete;
  Filter& operator=(Filter const&) = delete;
  /// Processes the input value and returns the filtered output value.
  virtual T process(T in) = 0;
  /// Clears the internal state (delay lines) without changing the coefficients.
  /// Call after reconfiguring filter parameters via begin() to avoid transients
  /// from stale state.
  virtual void reset() {}
};

/**
 * @brief Passes the input through unchanged. Useful as a placeholder when a
 * Filter is required but no processing is desired.
 * @ingroup filter
 * @author pschatzmann
 * @tparam T sample type
 */
template <typename T = float>
class NoFilter : public Filter<T> {
 public:
  // construct without coefs
  NoFilter() = default;
  T process(T in) override { return in; }
};

/**
 * @brief Finite Impulse Response (FIR) filter. Performs convolution of the
 * input signal with a set of feedforward coefficients b[]. For integer types,
 * an optional scaling factor is applied to preserve precision. You can use
 * https://www.arc.id.au/FilterDesign.html to design the coefficients.
 * @ingroup filter
 * @author Pieter P tttapa  / pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (float, double, or integer types with a scaling factor)
 */
template <typename T = float>
class FIR : public Filter<T> {
 public:
  template <size_t B>
  FIR(const T (&b)[B], const T factor = 1.0)
      : lenB(B), factor(factor), needs_divide(factor != T(1)) {
    setValues(b);
  }

  template <size_t B>
  void setValues(const T (&b)[B]) {
    x.resize(lenB);
    coeff_b.resize(2 * lenB - 1);
    for (uint16_t i = 0; i < 2 * lenB - 1; i++) {
      coeff_b[i] = b[(2 * lenB - 1 - i) % lenB];
    }
  }

  void reset() override {
    i_b = 0;
    for (uint16_t i = 0; i < lenB; i++) x[i] = 0;
  }

  T process(T value) override {
    x[i_b] = value;
    T b_terms = 0;
    T* b_shift = &coeff_b[lenB - i_b - 1];
    for (uint16_t i = 0; i < lenB; i++) {
      b_terms += b_shift[i] * x[i];
    }
    i_b++;
    if (i_b == lenB) i_b = 0;

#ifdef USE_TYPETRAITS
    if (!(std::is_same<T, float>::value || std::is_same<T, double>::value)) {
      b_terms = b_terms / factor;
    }
#else
    // needs_divide is computed once in the constructor (not per sample) --
    // for a bounded fixed-point T like q1_14_t, comparing against a float
    // literal here on every process() call would otherwise force a
    // per-sample float construction/comparison.
    if (needs_divide) {
      b_terms = b_terms / factor;
    }
#endif
    return b_terms;
  }

 private:
  const uint16_t lenB;
  uint16_t i_b = 0;
  Vector<T> x;
  Vector<T> coeff_b;
  T factor;
  bool needs_divide;
};

/**
 * @brief Infinite Impulse Response (IIR) filter. Uses both feedforward (b[])
 * and feedback (a[]) coefficients. The a[0] coefficient is used to normalize
 * all other coefficients. For integer types, an optional scaling factor is
 * applied to preserve precision.
 * @ingroup filter
 * @author Pieter P tttapa  / pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (float, double, or integer types with a scaling factor)
 */
template <typename T = float>
class IIR : public Filter<T> {
 public:
  template <size_t B, size_t A>
  IIR(const T (&b)[B], const T (&_a)[A], T factor = 1.0)
      : factor(factor), needs_divide(factor != T(1)), lenB(B), lenA(A - 1) {
    x.resize(lenB);
    y.resize(lenA);
    coeff_b.resize(2 * lenB - 1);
    coeff_a.resize(2 * lenA - 1);
    T a0 = _a[0];
    const T* a = &_a[1];
    for (uint16_t i = 0; i < 2 * lenB - 1; i++) {
      coeff_b[i] = b[(2 * lenB - 1 - i) % lenB] / a0;
    }
    for (uint16_t i = 0; i < 2 * lenA - 1; i++) {
      coeff_a[i] = a[(2 * lenA - 2 - i) % lenA] / a0;
    }
  }

  void reset() override {
    i_b = 0;
    i_a = 0;
    for (uint16_t i = 0; i < lenB; i++) x[i] = 0;
    for (uint16_t i = 0; i < lenA; i++) y[i] = 0;
  }

  T process(T value) override {
    x[i_b] = value;
    T b_terms = 0;
    T* b_shift = &coeff_b[lenB - i_b - 1];

    T a_terms = 0;
    T* a_shift = &coeff_a[lenA - i_a - 1];

    for (uint16_t i = 0; i < lenB; i++) {
      b_terms += x[i] * b_shift[i];
    }
    for (uint16_t i = 0; i < lenA; i++) {
      a_terms += y[i] * a_shift[i];
    }

    T filtered = b_terms - a_terms;
    y[i_a] = filtered;
    i_b++;
    if (i_b == lenB) i_b = 0;
    i_a++;
    if (i_a == lenA) i_a = 0;

#ifdef USE_TYPETRAITS
    if (!(std::is_same<T, float>::value || std::is_same<T, double>::value)) {
      filtered = filtered / factor;
    }
#else
    // needs_divide is computed once in the constructor (not per sample) --
    // see FIR::process() for why.
    if (needs_divide) {
      filtered = filtered / factor;
    }
#endif
    return filtered;
  }

 private:
  T factor;
  bool needs_divide;
  const uint16_t lenB, lenA;
  uint16_t i_b = 0, i_a = 0;
  Vector<T> x;
  Vector<T> y;
  Vector<T> coeff_b;
  Vector<T> coeff_a;
};

/**
 * @brief Second-order IIR filter in Direct Form I. Maintains separate input
 * and output histories (x and y delay lines). Requires more memory than DF2
 * but is less susceptible to quantization issues with fixed-point arithmetic.
 * @ingroup filter
 * @author Pieter P tttapa  / pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (use float or double, not integer types)
 */
template <typename T = float>
class BiQuadDF1 : public Filter<T> {
 public:
  BiQuadDF1(const T (&b)[3], const T (&a)[3])
      : b_0(b[0] / a[0]),
        b_1(b[1] / a[0]),
        b_2(b[2] / a[0]),
        a_1(a[1] / a[0]),
        a_2(a[2] / a[0]) {}
  BiQuadDF1(const T (&b)[3], const T (&a)[2])
      : b_0(b[0]), b_1(b[1]), b_2(b[2]), a_1(a[0]), a_2(a[1]) {}
  BiQuadDF1(const T (&b)[3], const T (&a)[2], T gain)
      : b_0(gain * b[0]),
        b_1(gain * b[1]),
        b_2(gain * b[2]),
        a_1(a[0]),
        a_2(a[1]) {}
  BiQuadDF1(const T (&b)[3], const T (&a)[3], T gain)
      : b_0(gain * b[0] / a[0]),
        b_1(gain * b[1] / a[0]),
        b_2(gain * b[2] / a[0]),
        a_1(a[1] / a[0]),
        a_2(a[2] / a[0]) {}

  void reset() override { x_0 = x_1 = y_1 = y_2 = 0; }

  T process(T value) override {
    T x_2 = x_1;
    x_1 = x_0;
    x_0 = value;
    T b_terms = x_0 * b_0 + x_1 * b_1 + x_2 * b_2;
    T a_terms = y_1 * a_1 + y_2 * a_2;
    y_2 = y_1;
    y_1 = b_terms - a_terms;
    return y_1;
  }

 protected:
  T b_0 = 0;
  T b_1 = 0;
  T b_2 = 0;
  T a_1 = 0;
  T a_2 = 0;

  // allow constructor w/o parameter in subclasses
  BiQuadDF1() = default;

  T x_0 = 0;
  T x_1 = 0;
  T y_1 = 0;
  T y_2 = 0;
};

/**
 * @brief Second-order IIR filter in Direct Form II. Uses a single delay line,
 * requiring less memory than DF1. This is the base class for the ready-to-use
 * filter types (LowPassFilter, HighPassFilter, BandPassFilter, NotchFilter,
 * LowShelfFilter, HighShelfFilter) which compute their coefficients from
 * frequency, sample rate and Q/gain parameters.
 * @ingroup filter
 * @author Pieter P tttapa  / pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (use float or double, not integer types)
 */
template <typename T = float>
class BiQuadDF2 : public Filter<T> {
 public:
  BiQuadDF2(const T (&b)[3], const T (&a)[3])
      : b_0(b[0] / a[0]),
        b_1(b[1] / a[0]),
        b_2(b[2] / a[0]),
        a_1(a[1] / a[0]),
        a_2(a[2] / a[0]) {}
  BiQuadDF2(const T (&b)[3], const T (&a)[2])
      : b_0(b[0]), b_1(b[1]), b_2(b[2]), a_1(a[0]), a_2(a[1]) {}
  BiQuadDF2(const T (&b)[3], const T (&a)[2], T gain)
      : b_0(gain * b[0]),
        b_1(gain * b[1]),
        b_2(gain * b[2]),
        a_1(a[0]),
        a_2(a[1]) {}
  BiQuadDF2(const T (&b)[3], const T (&a)[3], T gain)
      : b_0(gain * b[0] / a[0]),
        b_1(gain * b[1] / a[0]),
        b_2(gain * b[2] / a[0]),
        a_1(a[1] / a[0]),
        a_2(a[2] / a[0]) {}

  void reset() override { w_0 = w_1 = 0; }

  T process(T value) override {
    T w_2 = w_1;
    w_1 = w_0;
    w_0 = value - a_1 * w_1 - a_2 * w_2;
    T y = b_0 * w_0 + b_1 * w_1 + b_2 * w_2;
    return y;
  }

 protected:
  T b_0 = 0;
  T b_1 = 0;
  T b_2 = 0;
  T a_1 = 0;
  T a_2 = 0;

  // allow constructor w/o parameter in subclasses
  BiQuadDF2() = default;

  T w_0 = 0;
  T w_1 = 0;
};

/**
 * @brief Biquad coefficients, always computed in float regardless of the
 * filter's sample type T. Intermediate values like the angular cutoff
 * frequency w0 range up to PI (~3.14) at Nyquist, which overflows bounded
 * fixed-point sample types like q1_14_t (range ~[-2,2)); only the final,
 * range-safe coefficients (typically within [-2,2) for a stable filter) get
 * cast into T when assigned to a BiQuadDF1/BiQuadDF2 instance.
 * @ingroup filter
 */
struct BiQuadCoeffs {
  float b_0, b_1, b_2, a_1, a_2;
};

/**
 * @brief Warns (once per call, via LOGE) about any coefficient that doesn't
 * survive being stored in T. This is expected/harmless for float or double
 * (T can represent the value, roundtrip is exact); it's a real problem for a
 * bounded fixed-point type like q1_14_t (range ~[-2,2)), where a coefficient
 * outside that range silently saturates instead of erroring, quietly
 * producing a wrong filter -- e.g. shelf filters, whose coefficients scale
 * with the linear gain factor and can exceed +-2 even at modest gains (a
 * limitation of q1_14_t's 1-bit-of-headroom range, not of DF1 vs DF2; see
 * LowShelfFilterDF1/HighShelfFilterDF1).
 */
template <typename T>
void checkCoeffRange(const BiQuadCoeffs &c) {
  auto check = [](const char *name, float value) {
    T t = value;
    float roundtrip = (float)t;
    // allow normal quantization noise (e.g. ~1/16384 for q1_14_t); anything
    // larger indicates the value was clamped/saturated, not just rounded.
    if (fabs(roundtrip - value) > fabs(value) * 0.01f + 1e-4f) {
      LOGE(
          "Filter coefficient %s=%f does not fit in the range of T (stored "
          "as %f) - the filter will be inaccurate",
          name, value, roundtrip);
    }
  };
  check("b_0", c.b_0);
  check("b_1", c.b_1);
  check("b_2", c.b_2);
  check("a_1", c.a_1);
  check("a_2", c.a_2);
}

/// Computes the b0/b1/b2/a1/a2 biquad coefficients for a low-pass filter.
inline BiQuadCoeffs calculateLowPassCoeffs(float frequency, float sampleRate,
                                            float q) {
  float w0 = frequency * (2.0f * PI / sampleRate);
  float sinW0 = sin(w0);
  float alpha = sinW0 / (q * 2.0f);
  float cosW0 = cos(w0);
  float scale = 1.0f / (1.0f + alpha);
  BiQuadCoeffs c;
  c.b_0 = ((1.0f - cosW0) / 2.0f) * scale;
  c.b_1 = (1.0f - cosW0) * scale;
  c.b_2 = c.b_0;
  c.a_1 = (-2.0f * cosW0) * scale;
  c.a_2 = (1.0f - alpha) * scale;
  return c;
}

/**
 * @brief Second-order low-pass filter (BiQuad DF2). Attenuates frequencies
 * above the cutoff frequency. Coefficients are derived from the Audio EQ
 * Cookbook.
 * @param frequency cutoff frequency in Hz
 * @param sampleRate sample rate in Hz
 * @param q quality factor (default 0.7071 = Butterworth, no resonance peak)
 * @ingroup filter
 * @author  pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (use float or double; for fixed-point types like
 * q1_14_t use LowPassFilterDF1 instead -- DF2's single delay line needs much
 * more dynamic range than the signal for low cutoff/sampleRate ratios and
 * will saturate/misbehave in a bounded fixed-point type)
 */

template <typename T = float>
class LowPassFilter : public BiQuadDF2<T> {
 public:
  LowPassFilter() = default;
  LowPassFilter(float frequency, float sampleRate, float q = 0.7071f)
      : BiQuadDF2<T>() {
    begin(frequency, sampleRate, q);
  }
  void begin(float frequency, float sampleRate, float q = 0.7071f) {
    BiQuadCoeffs c = calculateLowPassCoeffs(frequency, sampleRate, q);
    checkCoeffRange<T>(c);
    BiQuadDF2<T>::b_0 = c.b_0;
    BiQuadDF2<T>::b_1 = c.b_1;
    BiQuadDF2<T>::b_2 = c.b_2;
    BiQuadDF2<T>::a_1 = c.a_1;
    BiQuadDF2<T>::a_2 = c.a_2;
  }
};

/**
 * @brief Second-order low-pass filter (BiQuad DF1). Same coefficients as
 * LowPassFilter, but built on BiQuadDF1, whose separate x[]/y[] delay lines
 * stay bounded to the actual signal range -- unlike DF2's single delay line,
 * which can need far more dynamic range than the signal for low cutoff/
 * sampleRate ratios. Use this (not LowPassFilter) when T is a bounded
 * fixed-point type like q1_14_t.
 * @param frequency cutoff frequency in Hz
 * @param sampleRate sample rate in Hz
 * @param q quality factor (default 0.7071 = Butterworth, no resonance peak)
 * @ingroup filter
 * @author  pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (float, double, or a bounded fixed-point type like
 * q1_14_t)
 */

template <typename T = float>
class LowPassFilterDF1 : public BiQuadDF1<T> {
 public:
  LowPassFilterDF1() = default;
  LowPassFilterDF1(float frequency, float sampleRate, float q = 0.7071f)
      : BiQuadDF1<T>() {
    begin(frequency, sampleRate, q);
  }
  void begin(float frequency, float sampleRate, float q = 0.7071f) {
    BiQuadCoeffs c = calculateLowPassCoeffs(frequency, sampleRate, q);
    checkCoeffRange<T>(c);
    BiQuadDF1<T>::b_0 = c.b_0;
    BiQuadDF1<T>::b_1 = c.b_1;
    BiQuadDF1<T>::b_2 = c.b_2;
    BiQuadDF1<T>::a_1 = c.a_1;
    BiQuadDF1<T>::a_2 = c.a_2;
  }
};

/// Computes the b0/b1/b2/a1/a2 biquad coefficients for a high-pass filter.
inline BiQuadCoeffs calculateHighPassCoeffs(float frequency, float sampleRate,
                                             float q) {
  float w0 = frequency * (2.0f * PI / sampleRate);
  float sinW0 = sin(w0);
  float alpha = sinW0 / (q * 2.0f);
  float cosW0 = cos(w0);
  float scale = 1.0f / (1.0f + alpha);
  BiQuadCoeffs c;
  c.b_0 = ((1.0f + cosW0) / 2.0f) * scale;
  c.b_1 = -(1.0f + cosW0) * scale;
  c.b_2 = c.b_0;
  c.a_1 = (-2.0f * cosW0) * scale;
  c.a_2 = (1.0f - alpha) * scale;
  return c;
}

/**
 * @brief Second-order high-pass filter (BiQuad DF2). Attenuates frequencies
 * below the cutoff frequency. Coefficients are derived from the Audio EQ
 * Cookbook.
 * @param frequency cutoff frequency in Hz
 * @param sampleRate sample rate in Hz
 * @param q quality factor (default 0.7071 = Butterworth)
 * @ingroup filter
 * @author pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (use float or double; for fixed-point types like
 * q1_14_t use HighPassFilterDF1 instead -- see LowPassFilter for why)
 */

template <typename T = float>
class HighPassFilter : public BiQuadDF2<T> {
 public:
  HighPassFilter() = default;
  HighPassFilter(float frequency, float sampleRate, float q = 0.7071f)
      : BiQuadDF2<T>() {
    begin(frequency, sampleRate, q);
  }
  void begin(float frequency, float sampleRate, float q = 0.7071f) {
    BiQuadCoeffs c = calculateHighPassCoeffs(frequency, sampleRate, q);
    checkCoeffRange<T>(c);
    BiQuadDF2<T>::b_0 = c.b_0;
    BiQuadDF2<T>::b_1 = c.b_1;
    BiQuadDF2<T>::b_2 = c.b_2;
    BiQuadDF2<T>::a_1 = c.a_1;
    BiQuadDF2<T>::a_2 = c.a_2;
  }
};

/**
 * @brief Second-order high-pass filter (BiQuad DF1). Same coefficients as
 * HighPassFilter; use this (not HighPassFilter) when T is a bounded
 * fixed-point type like q1_14_t -- see LowPassFilterDF1 for why.
 * @param frequency cutoff frequency in Hz
 * @param sampleRate sample rate in Hz
 * @param q quality factor (default 0.7071 = Butterworth)
 * @ingroup filter
 * @author pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (float, double, or a bounded fixed-point type like
 * q1_14_t)
 */

template <typename T = float>
class HighPassFilterDF1 : public BiQuadDF1<T> {
 public:
  HighPassFilterDF1() = default;
  HighPassFilterDF1(float frequency, float sampleRate, float q = 0.7071f)
      : BiQuadDF1<T>() {
    begin(frequency, sampleRate, q);
  }
  void begin(float frequency, float sampleRate, float q = 0.7071f) {
    BiQuadCoeffs c = calculateHighPassCoeffs(frequency, sampleRate, q);
    checkCoeffRange<T>(c);
    BiQuadDF1<T>::b_0 = c.b_0;
    BiQuadDF1<T>::b_1 = c.b_1;
    BiQuadDF1<T>::b_2 = c.b_2;
    BiQuadDF1<T>::a_1 = c.a_1;
    BiQuadDF1<T>::a_2 = c.a_2;
  }
};

/// Computes the b0/b1/b2/a1/a2 biquad coefficients for a band-pass filter.
inline BiQuadCoeffs calculateBandPassCoeffs(float frequency, float sampleRate,
                                             float q) {
  float w0 = frequency * (2.0f * PI / sampleRate);
  float sinW0 = sin(w0);
  float alpha = sinW0 / (q * 2.0f);
  float cosW0 = cos(w0);
  float scale = 1.0f / (1.0f + alpha);
  BiQuadCoeffs c;
  c.b_0 = alpha * scale;
  c.b_1 = 0;
  c.b_2 = (-alpha) * scale;
  c.a_1 = (-2.0f * cosW0) * scale;
  c.a_2 = (1.0f - alpha) * scale;
  return c;
}

/**
 * @brief Second-order band-pass filter (BiQuad DF2). Passes frequencies near
 * the center frequency and attenuates those further away. The bandwidth is
 * controlled by the Q factor: higher Q produces a narrower passband.
 * Coefficients are derived from the Audio EQ Cookbook.
 * @param frequency center frequency in Hz
 * @param sampleRate sample rate in Hz
 * @param q quality factor (default 1.0; higher values narrow the passband)
 * @ingroup filter
 * @author  pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (use float or double; for fixed-point types like
 * q1_14_t use BandPassFilterDF1 instead -- see LowPassFilter for why)
 */

template <typename T = float>
class BandPassFilter : public BiQuadDF2<T> {
 public:
  BandPassFilter() = default;
  BandPassFilter(float frequency, float sampleRate, float q = 1.0)
      : BiQuadDF2<T>() {
    begin(frequency, sampleRate, q);
  }
  void begin(float frequency, float sampleRate, float q = 1.0) {
    BiQuadCoeffs c = calculateBandPassCoeffs(frequency, sampleRate, q);
    checkCoeffRange<T>(c);
    BiQuadDF2<T>::b_0 = c.b_0;
    BiQuadDF2<T>::b_1 = c.b_1;
    BiQuadDF2<T>::b_2 = c.b_2;
    BiQuadDF2<T>::a_1 = c.a_1;
    BiQuadDF2<T>::a_2 = c.a_2;
  }
};

/**
 * @brief Second-order band-pass filter (BiQuad DF1). Same coefficients as
 * BandPassFilter; use this (not BandPassFilter) when T is a bounded
 * fixed-point type like q1_14_t -- see LowPassFilterDF1 for why.
 * @param frequency center frequency in Hz
 * @param sampleRate sample rate in Hz
 * @param q quality factor (default 1.0; higher values narrow the passband)
 * @ingroup filter
 * @author  pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (float, double, or a bounded fixed-point type like
 * q1_14_t)
 */

template <typename T = float>
class BandPassFilterDF1 : public BiQuadDF1<T> {
 public:
  BandPassFilterDF1() = default;
  BandPassFilterDF1(float frequency, float sampleRate, float q = 1.0)
      : BiQuadDF1<T>() {
    begin(frequency, sampleRate, q);
  }
  void begin(float frequency, float sampleRate, float q = 1.0) {
    BiQuadCoeffs c = calculateBandPassCoeffs(frequency, sampleRate, q);
    checkCoeffRange<T>(c);
    BiQuadDF1<T>::b_0 = c.b_0;
    BiQuadDF1<T>::b_1 = c.b_1;
    BiQuadDF1<T>::b_2 = c.b_2;
    BiQuadDF1<T>::a_1 = c.a_1;
    BiQuadDF1<T>::a_2 = c.a_2;
  }
};


/// Computes the b0/b1/b2/a1/a2 biquad coefficients for a notch (band-reject)
/// filter.
inline BiQuadCoeffs calculateNotchCoeffs(float frequency, float sampleRate,
                                          float q) {
  float w0 = frequency * (2.0f * PI / sampleRate);
  float sinW0 = sin(w0);
  float alpha = sinW0 / (q * 2.0f);
  float cosW0 = cos(w0);
  float scale = 1.0f / (1.0f + alpha);
  BiQuadCoeffs c;
  c.b_0 = scale;
  c.b_1 = (-2.0f * cosW0) * scale;
  c.b_2 = c.b_0;
  c.a_1 = (-2.0f * cosW0) * scale;
  c.a_2 = (1.0f - alpha) * scale;
  return c;
}

/**
 * @brief Second-order notch (band-reject) filter (BiQuad DF2). Rejects
 * frequencies near the center frequency and passes those further away. Useful
 * for removing a specific unwanted frequency (e.g. mains hum at 50/60 Hz).
 * Coefficients are derived from the Audio EQ Cookbook.
 * @param frequency center frequency to reject in Hz
 * @param sampleRate sample rate in Hz
 * @param q quality factor (default 1.0; higher values narrow the rejection
 * band)
 * @ingroup filter
 * @author  pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (use float or double; for fixed-point types like
 * q1_14_t use NotchFilterDF1 instead -- see LowPassFilter for why)
 */

template <typename T = float>
class NotchFilter : public BiQuadDF2<T> {
 public:
  NotchFilter() = default;
  NotchFilter(float frequency, float sampleRate, float q = 1.0)
      : BiQuadDF2<T>() {
    begin(frequency, sampleRate, q);
  }

  void begin(float frequency, float sampleRate, float q = 1.0) {
    BiQuadCoeffs c = calculateNotchCoeffs(frequency, sampleRate, q);
    checkCoeffRange<T>(c);
    BiQuadDF2<T>::b_0 = c.b_0;
    BiQuadDF2<T>::b_1 = c.b_1;
    BiQuadDF2<T>::b_2 = c.b_2;
    BiQuadDF2<T>::a_1 = c.a_1;
    BiQuadDF2<T>::a_2 = c.a_2;
  }
};

/**
 * @brief Second-order notch (band-reject) filter (BiQuad DF1). Same
 * coefficients as NotchFilter; use this (not NotchFilter) when T is a
 * bounded fixed-point type like q1_14_t -- see LowPassFilterDF1 for why.
 * @param frequency center frequency to reject in Hz
 * @param sampleRate sample rate in Hz
 * @param q quality factor (default 1.0; higher values narrow the rejection
 * band)
 * @ingroup filter
 * @author  pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (float, double, or a bounded fixed-point type like
 * q1_14_t)
 */

template <typename T = float>
class NotchFilterDF1 : public BiQuadDF1<T> {
 public:
  NotchFilterDF1() = default;
  NotchFilterDF1(float frequency, float sampleRate, float q = 1.0)
      : BiQuadDF1<T>() {
    begin(frequency, sampleRate, q);
  }

  void begin(float frequency, float sampleRate, float q = 1.0) {
    BiQuadCoeffs c = calculateNotchCoeffs(frequency, sampleRate, q);
    checkCoeffRange<T>(c);
    BiQuadDF1<T>::b_0 = c.b_0;
    BiQuadDF1<T>::b_1 = c.b_1;
    BiQuadDF1<T>::b_2 = c.b_2;
    BiQuadDF1<T>::a_1 = c.a_1;
    BiQuadDF1<T>::a_2 = c.a_2;
  }
};

/// Computes the b0/b1/b2/a1/a2 biquad coefficients for a low-shelf filter.
inline BiQuadCoeffs calculateLowShelfCoeffs(float frequency, float sampleRate,
                                             float gain, float slope) {
  float a = pow(10.0f, gain / 40.0f);
  float w0 = frequency * (2.0f * PI / sampleRate);
  float sinW0 = sin(w0);
  // float alpha = (sinW0 * sqrt((a+1/a)*(1/slope-1)+2) ) / 2.0;
  float cosW0 = cos(w0);
  // generate three helper-values (intermediate results):
  float sinsq =
      sinW0 * sqrt((pow(a, 2.0f) + 1.0f) * (1.0f / slope - 1.0f) + 2.0f * a);
  float aMinus = (a - 1.0f) * cosW0;
  float aPlus = (a + 1.0f) * cosW0;
  float scale = 1.0f / ((a + 1.0f) + aMinus + sinsq);
  BiQuadCoeffs c;
  c.b_0 = a * ((a + 1.0f) - aMinus + sinsq) * scale;
  c.b_1 = 2.0f * a * ((a - 1.0f) - aPlus) * scale;
  c.b_2 = a * ((a + 1.0f) - aMinus - sinsq) * scale;
  c.a_1 = -2.0f * ((a - 1.0f) + aPlus) * scale;
  c.a_2 = ((a + 1.0f) + aMinus - sinsq) * scale;
  return c;
}

/**
 * @brief Second-order low-shelf filter (BiQuad DF2). Boosts or cuts frequencies
 * below the shelf frequency by the specified gain (in dB) while leaving higher
 * frequencies unchanged. Commonly used in tone controls and equalization.
 * Coefficients are derived from the Audio EQ Cookbook.
 * @param frequency shelf transition frequency in Hz
 * @param sampleRate sample rate in Hz
 * @param gain boost/cut in dB (positive = boost, negative = cut)
 * @param slope shelf slope (default 1.0; values < 1.0 give a gentler slope)
 * @ingroup filter
 * @author  pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (use float or double). NOTE: unlike the other
 * filters here, LowShelfFilterDF1 is NOT a reliable fix for bounded
 * fixed-point types like q1_14_t: shelf coefficients scale with the linear
 * gain factor 10^(gain_dB/40) and can exceed a bounded type's range even at
 * modest gain settings, independent of DF1 vs DF2 -- this is a coefficient
 * headroom limitation, not the DF2 state-headroom issue DF1 solves for the
 * other filter types. begin() logs an error (LOGE) via checkCoeffRange() if
 * this happens; check for that warning rather than assuming it works.
 */

template <typename T = float>
class LowShelfFilter : public BiQuadDF2<T> {
 public:
  LowShelfFilter() = default;
  LowShelfFilter(float frequency, float sampleRate, float gain,
                 float slope = 1.0f)
      : BiQuadDF2<T>() {
    begin(frequency, sampleRate, gain, slope);
  }

  void begin(float frequency, float sampleRate, float gain,
             float slope = 1.0f) {
    BiQuadCoeffs c = calculateLowShelfCoeffs(frequency, sampleRate, gain, slope);
    checkCoeffRange<T>(c);
    BiQuadDF2<T>::b_0 = c.b_0;
    BiQuadDF2<T>::b_1 = c.b_1;
    BiQuadDF2<T>::b_2 = c.b_2;
    BiQuadDF2<T>::a_1 = c.a_1;
    BiQuadDF2<T>::a_2 = c.a_2;
  }
};

/**
 * @brief Second-order low-shelf filter (BiQuad DF1). Same coefficients as
 * LowShelfFilter. NOTE: switching to DF1 does NOT make this reliable for
 * bounded fixed-point types like q1_14_t -- shelf coefficients scale with
 * the linear gain factor and can exceed the type's range even at modest
 * gain settings; this is a coefficient headroom problem, not the DF2
 * state-headroom problem DF1 fixes elsewhere. Watch for the LOGE warning
 * from checkCoeffRange() in begin().
 * @param frequency shelf transition frequency in Hz
 * @param sampleRate sample rate in Hz
 * @param gain boost/cut in dB (positive = boost, negative = cut)
 * @param slope shelf slope (default 1.0; values < 1.0 give a gentler slope)
 * @ingroup filter
 * @author  pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (use float or double; see note above for bounded
 * fixed-point types)
 */

template <typename T = float>
class LowShelfFilterDF1 : public BiQuadDF1<T> {
 public:
  LowShelfFilterDF1() = default;
  LowShelfFilterDF1(float frequency, float sampleRate, float gain,
                     float slope = 1.0f)
      : BiQuadDF1<T>() {
    begin(frequency, sampleRate, gain, slope);
  }

  void begin(float frequency, float sampleRate, float gain,
             float slope = 1.0f) {
    BiQuadCoeffs c = calculateLowShelfCoeffs(frequency, sampleRate, gain, slope);
    checkCoeffRange<T>(c);
    BiQuadDF1<T>::b_0 = c.b_0;
    BiQuadDF1<T>::b_1 = c.b_1;
    BiQuadDF1<T>::b_2 = c.b_2;
    BiQuadDF1<T>::a_1 = c.a_1;
    BiQuadDF1<T>::a_2 = c.a_2;
  }
};

/// Computes the b0/b1/b2/a1/a2 biquad coefficients for a high-shelf filter.
inline BiQuadCoeffs calculateHighShelfCoeffs(float frequency,
                                              float sampleRate, float gain,
                                              float slope) {
  float a = pow(10.0f, gain / 40.0f);
  float w0 = frequency * (2.0f * PI / sampleRate);
  float sinW0 = sin(w0);
  // float alpha = (sinW0 * sqrt((a+1/a)*(1/slope-1)+2) ) / 2.0;
  float cosW0 = cos(w0);
  // generate three helper-values (intermediate results):
  float sinsq =
      sinW0 * sqrt((pow(a, 2.0f) + 1.0f) * (1.0f / slope - 1.0f) + 2.0f * a);
  float aMinus = (a - 1.0f) * cosW0;
  float aPlus = (a + 1.0f) * cosW0;
  float scale = 1.0f / ((a + 1.0f) - aMinus + sinsq);
  BiQuadCoeffs c;
  c.b_0 = a * ((a + 1.0f) + aMinus + sinsq) * scale;
  c.b_1 = -2.0f * a * ((a - 1.0f) + aPlus) * scale;
  c.b_2 = a * ((a + 1.0f) + aMinus - sinsq) * scale;
  c.a_1 = 2.0f * ((a - 1.0f) - aPlus) * scale;
  c.a_2 = ((a + 1.0f) - aMinus - sinsq) * scale;
  return c;
}

/**
 * @brief Second-order high-shelf filter (BiQuad DF2). Boosts or cuts
 * frequencies above the shelf frequency by the specified gain (in dB) while
 * leaving lower frequencies unchanged. Commonly used in tone controls and
 * equalization.
 * Coefficients are derived from the Audio EQ Cookbook.
 * @param frequency shelf transition frequency in Hz
 * @param sampleRate sample rate in Hz
 * @param gain boost/cut in dB (positive = boost, negative = cut)
 * @param slope shelf slope (default 1.0; values < 1.0 give a gentler slope)
 * @ingroup filter
 * @author pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (use float or double). NOTE: q1_14_t (or any
 * fixed-point type with only ~1 bit of headroom above +-1.0) does not work
 * here -- HighShelfFilterDF1 does NOT fix this the way it fixes the other
 * filters. Its coefficients scale with the linear gain factor
 * 10^(gain_dB/40) and, for a low shelf frequency relative to sampleRate,
 * already exceed a q1_14_t-sized range at just +1dB of gain (measured, not
 * a theoretical edge case). begin() logs an error (LOGE) via
 * checkCoeffRange() when this happens.
 */

template <typename T = float>
class HighShelfFilter : public BiQuadDF2<T> {
 public:
  HighShelfFilter() = default;
  HighShelfFilter(float frequency, float sampleRate, float gain,
                  float slope = 1.0f)
      : BiQuadDF2<T>() {
    begin(frequency, sampleRate, gain, slope);
  }
  void begin(float frequency, float sampleRate, float gain,
             float slope = 1.0f) {
    BiQuadCoeffs c = calculateHighShelfCoeffs(frequency, sampleRate, gain, slope);
    checkCoeffRange<T>(c);
    BiQuadDF2<T>::b_0 = c.b_0;
    BiQuadDF2<T>::b_1 = c.b_1;
    BiQuadDF2<T>::b_2 = c.b_2;
    BiQuadDF2<T>::a_1 = c.a_1;
    BiQuadDF2<T>::a_2 = c.a_2;
  }
};

/**
 * @brief Second-order high-shelf filter (BiQuad DF1). Same coefficients as
 * HighShelfFilter. NOTE: unlike the other DF1 filters here, this is NOT a
 * working fix for q1_14_t (or similar bounded fixed-point types) -- see the
 * @tparam note on HighShelfFilter for measured numbers. Watch for the LOGE
 * warning from checkCoeffRange() in begin().
 * @param frequency shelf transition frequency in Hz
 * @param sampleRate sample rate in Hz
 * @param gain boost/cut in dB (positive = boost, negative = cut)
 * @param slope shelf slope (default 1.0; values < 1.0 give a gentler slope)
 * @ingroup filter
 * @author pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (use float or double; see note above for bounded
 * fixed-point types)
 */

template <typename T = float>
class HighShelfFilterDF1 : public BiQuadDF1<T> {
 public:
  HighShelfFilterDF1() = default;
  HighShelfFilterDF1(float frequency, float sampleRate, float gain,
                      float slope = 1.0f)
      : BiQuadDF1<T>() {
    begin(frequency, sampleRate, gain, slope);
  }
  void begin(float frequency, float sampleRate, float gain,
             float slope = 1.0f) {
    BiQuadCoeffs c = calculateHighShelfCoeffs(frequency, sampleRate, gain, slope);
    checkCoeffRange<T>(c);
    BiQuadDF1<T>::b_0 = c.b_0;
    BiQuadDF1<T>::b_1 = c.b_1;
    BiQuadDF1<T>::b_2 = c.b_2;
    BiQuadDF1<T>::a_1 = c.a_1;
    BiQuadDF1<T>::a_2 = c.a_2;
  }
};

/**
 * @brief Second Order Sections (SOS) filter — a cascade of N BiQuad DF2
 * stages. Higher-order filters should be decomposed into second-order sections
 * to avoid numerical instability. Each section has its own b[3]/a[3]
 * coefficients and optional gain. Tools like scipy.signal.butter(...,
 * output='sos') or MATLAB's tf2sos produce the required coefficient arrays.
 * @ingroup filter
 * @author Pieter P tttapa  / pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T sample type (use float or double)
 * @tparam N number of second-order sections
 */

template <typename T, size_t N>
class SOSFilter : public Filter<T> {
 public:
  SOSFilter(const T (&b)[N][3], const T (&a)[N][3], const T (&gain)[N]) {
    for (size_t i = 0; i < N; i++)
      filters[i] = new BiQuadDF2<T>(b[i], a[i], gain[i]);
  }
  SOSFilter(const T (&sos)[N][6], const T (&gain)[N]) {
    for (size_t i = 0; i < N; i++) {
      T b[3];
      T a[3];
      copy(b, &sos[i][0]);
      copy(a, &sos[i][3]);
      filters[i] = new BiQuadDF2<T>(b, a, gain[i]);
    }
  }
  SOSFilter(const T (&b)[N][3], const T (&a)[N][2], const T (&gain)[N]) {
    for (size_t i = 0; i < N; i++)
      filters[i] = new BiQuadDF2<T>(b[i], a[i], gain[i]);
  }
  SOSFilter(const T (&b)[N][3], const T (&a)[N][2]) {
    for (size_t i = 0; i < N; i++) filters[i] = new BiQuadDF2<T>(b[i], a[i]);
  }
  SOSFilter(const T (&b)[N][3], const T (&a)[N][3]) {
    for (size_t i = 0; i < N; i++) filters[i] = new BiQuadDF2<T>(b[i], a[i]);
  }
  SOSFilter(SOSFilter const&) = delete;
  SOSFilter& operator=(SOSFilter const&) = delete;
  ~SOSFilter() {
    for (size_t i = 0; i < N; i++) delete filters[i];
  }
  void reset() override {
    for (Filter<T>*& filter : filters) filter->reset();
  }
  T process(T value) override {
    for (Filter<T>*& filter : filters) value = filter->process(value);
    return value;
  }

 private:
  Filter<T>* filters[N];
  template <size_t M>
  void copy(T (&dest)[M], const T* src) {
    for (size_t i = 0; i < M; i++) dest[i] = src[i];
  }
};

/**
 * @brief A cascade of N arbitrary filters applied in series. Each sample is
 * passed through all filters in order. The caller owns the filter pointers and
 * must ensure they outlive the chain.
 * @ingroup filter
 * @tparam T sample type
 * @tparam N number of filters in the chain
 */
template <typename T, size_t N>
class FilterChain : public Filter<T> {
 public:
  FilterChain(Filter<T>* (&&filters)[N]) {
    for (size_t i = 0; i < N; i++) {
      this->filters[i] = filters[i];
    }
  }

  void reset() override {
    for (Filter<T>*& filter : filters) {
      if (filter != nullptr) filter->reset();
    }
  }

  T process(T value) override {
    for (Filter<T>*& filter : filters) {
      if (filter != nullptr) {
        value = filter->process(value);
      }
    }
    return value;
  }

 private:
  Filter<T>* filters[N] = {0};
};

}  // namespace audio_tools
