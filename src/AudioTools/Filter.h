#pragma once
#include "AudioTools/Converter.h"
#include "AudioConfig.h"
#ifdef USE_TYPETRAITS
#include <type_traits>
#endif
namespace audio_tools {

/**
 * @brief Abstract filter interface definition;
 * @author pschatzmann
 */
template <typename T>
class Filter {
 public:
  // construct without coefs
  Filter() = default;
  virtual ~Filter() = default;
  virtual T process(T in) = 0;
};

/**
 * @brief No change to the input
 * @author pschatzmann
 *
 * @tparam T
 */
template <typename T>
class NoFilter : Filter<T> {
 public:
  // construct without coefs
  NoFilter() = default;
  virtual T process(T in){return in;}
};

/**
 * @brief FIR Filter
 * Converted from
 * https://github.com/sebnil/FIR-filter-Arduino-Library/tree/master/src
 * You can use https://www.arc.id.au/FilterDesign.html to design the filter
 * @author Pieter P tttapa  / pschatzmann
 * @copyright GNU General Public License v3.0
 */
template <typename T>
class FIR : public Filter<T> {
  public:
    template  <size_t B>
    FIR(const T (&b)[B], const T factor=1.0) : lenB(B), factor(factor) {
      x = new T[lenB]();
      coeff_b = new T[2*lenB-1];
      for (uint16_t i = 0; i < 2*lenB-1; i++) {
        coeff_b[i] = b[(2*lenB - 1 - i)%lenB];
      } 
    }
    ~FIR() {
      delete[] x;
      delete[] coeff_b;
    }
    T process(T value) {
      x[i_b] = value;
      T b_terms = 0;
      T *b_shift = &coeff_b[lenB - i_b - 1];
      for (uint8_t i = 0; i < lenB; i++) {
        b_terms += b_shift[i] * x[i] ; 
      }
      i_b++;
      if(i_b == lenB)
        i_b = 0;

#ifdef USE_TYPETRAITS
      if (!(std::is_same<T, float>::value || std::is_same<T, double>::value)) {
        b_terms = b_terms / factor;
      }
#else
      if (factor!=1.0) {
        b_terms = b_terms / factor;
      }
#endif
      return b_terms;
    }
  private:
    const uint8_t lenB;
    uint8_t i_b = 0;
    T *x;
    T *coeff_b;
    T factor;
};


/**
 * @brief IIRFilter
 * Converted from https://github.com/tttapa/Filters/blob/master/src/IIRFilter.h
 * @author Pieter P tttapa  / pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T
 */
template <typename T>
class IIR : public Filter<T> {
 public:
  template <size_t B, size_t A>
  IIR(const T (&b)[B], const T (&_a)[A], T factor=1.0) : factor(factor), lenB(B), lenA(A - 1) {
    x = new T[lenB]();
    y = new T[lenA]();
    coeff_b = new T[2 * lenB - 1];
    coeff_a = new T[2 * lenA - 1];
    T a0 = _a[0];
    const T *a = &_a[1];
    for (uint16_t i = 0; i < 2 * lenB - 1; i++) {
      coeff_b[i] = b[(2 * lenB - 1 - i) % lenB] / a0;
    }
    for (uint16_t i = 0; i < 2 * lenA - 1; i++) {
      coeff_a[i] = a[(2 * lenA - 2 - i) % lenA] / a0;
    }
  }

  ~IIR() {
    delete[] x;
    delete[] y;
    delete[] coeff_a;
    delete[] coeff_b;
  }

  T process(T value) {
    x[i_b] = value;
    T b_terms = 0;
    T *b_shift = &coeff_b[lenB - i_b - 1];

    T a_terms = 0;
    T *a_shift = &coeff_a[lenA - i_a - 1];

    for (uint8_t i = 0; i < lenB; i++) {
      b_terms += x[i] * b_shift[i];
    }
    for (uint8_t i = 0; i < lenA; i++) {
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
    if (factor!=1.0) {
      filtered = filtered / factor;
    }
#endif
    return filtered;
  }

 private:
  T factor;
  const uint8_t lenB, lenA;
  uint8_t i_b = 0, i_a = 0;
  T *x;
  T *y;
  T *coeff_b;
  T *coeff_a;
};

/**
 * @brief Biquad DF1 Filter. 
 * converted from https://github.com/tttapa/Filters/blob/master/src/BiQuad.h
 * Use float or double (and not a integer type) as type parameter
 * @author Pieter P tttapa  / pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T 
 */
template <typename T>
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

  T process(T value) {
    T x_2 = x_1;
    x_1 = x_0;
    x_0 = value;
    T b_terms = x_0 * b_0 + x_1 * b_1 + x_2 * b_2;
    T a_terms = y_1 * a_1 + y_2 * a_2;
    y_2 = y_1;
    y_1 = b_terms - a_terms;
    return y_1;
  }

 private:
  const T b_0;
  const T b_1;
  const T b_2;
  const T a_1;
  const T a_2;

  T x_0 = 0;
  T x_1 = 0;
  T y_1 = 0;
  T y_2 = 0;
};


/**
 * @brief Biquad DF2 Filter. When dealing with high-order IIR filters, they can get unstable.
 * To prevent this, BiQuadratic filters (second order) are used.
 * Converted from https://github.com/tttapa/Filters/blob/master/src/BiQuad.h
 * Use float or double (and not a integer type) as type parameter
 * @author Pieter P tttapa  / pschatzmann
 * @copyright GNU General Public License v3.0
 * @tparam T 
 */
template <typename T>
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

  T process(T value) {
    T w_2 = w_1;
    w_1 = w_0;
    w_0 = value - a_1 * w_1 - a_2 * w_2;
    T y = b_0 * w_0 + b_1 * w_1 + b_2 * w_2;
    return y;
  }

 private:
  const T b_0;
  const T b_1;
  const T b_2;
  const T a_1;
  const T a_2;

  T w_0 = 0;
  T w_1 = 0;
};

/**
 * Second Order Filter: Instead of manually cascading BiQuad filters, you can use a Second Order Sections filter (SOS).
 * converted from https://github.com/tttapa/Filters/blob/master/src/SOSFilter.h
 * Use float or double (and not a integer type) as type parameter
 * @author Pieter P tttapa  / pschatzmann
 * @copyright GNU General Public License v3.0
 */

template <typename T, size_t N>
class SOSFilter : public Filter<T>
{
  public:
    SOSFilter(const T (&b)[N][3], const T (&a)[N][3], const T (&gain)[N])
    {
        for (size_t i = 0; i < N; i++)
            filters[i] = new BiQuadDF2<T>(b[i], a[i], gain[i]);
    }
    SOSFilter(const T (&sos)[N][6], const T (&gain)[N])
    {
        for (size_t i = 0; i < N; i++) {
            T b[3];
            T a[3];
            copy(b, &sos[i][0]);
            copy(a, &sos[i][3]);
            filters[i] = new BiQuadDF2<T>(b, a, gain[i]);
        }
    }
    SOSFilter(const T (&b)[N][3], const T (&a)[N][2], const T (&gain)[N])
    {
        for (size_t i = 0; i < N; i++)
            filters[i] = new BiQuadDF2<T>(b[i], a[i], gain[i]);
    }
    SOSFilter(const T (&b)[N][3], const T (&a)[N][2])
    {
        for (size_t i = 0; i < N; i++)
            filters[i] = new BiQuadDF2<T>(b[i], a[i]);
    }
    SOSFilter(const T (&b)[N][3], const T (&a)[N][3])
    {
        for (size_t i = 0; i < N; i++)
            filters[i] = new BiQuadDF2<T>(b[i], a[i]);
    }
    ~SOSFilter()
    {
        for (size_t i = 0; i < N; i++)
            delete filters[i];
    }
    T process(T value)
    {
        for (Filter<T> *&filter : filters)
            value = filter->filter(value);
        return value;
    }

  private:
    Filter<T> *filters[N];
    template <size_t M>
    void copy(T (&dest)[M], const T *src) {
        for (size_t i = 0; i < M; i++)
            dest[i] = src[i];
    }
};

/**
 * @brief FilterChain - A Cascade of multiple filters
 * 
 * @tparam T 
 * @tparam N 
 */
template <typename T, size_t N>
class FilterChain : public Filter<T> {
  public:
    FilterChain(Filter<T> * (&&filters)[N])
    {
        for (size_t i = 0; i < N; i++) {
            this->filters[i] = filters[i];
        }
    }

    T process(T value)
    {
        for (Filter<T> *&filter : filters) {
            if (filter!=nullptr){
              value = filter->process(value);
            }
        }
        return value;
    }

  private:
    Filter<T> *filters[N] = {0};
};


/**
 * @brief Converter for 1 Channel which applies the indicated Filter
 * @author pschatzmann
 * @tparam T
 */
template <typename T>
class Converter1Channel : public BaseConverter<T> {
 public:
  Converter1Channel(Filter<T> &filter) { this->p_filter = &filter; }

  size_t convert(uint8_t *src, size_t size) {
    T *data = (T *)src;
    for (size_t j = 0; j < size; j++) {
      data[j] = p_filter->process(data[j]);
    }
    return size;
  }

 protected:
  Filter<T> *p_filter = nullptr;
};

/**
 * @brief Converter for n Channels which applies the indicated Filter
 * @author pschatzmann
 * @tparam T
 */
template <typename T, typename FT>
class ConverterNChannels : public BaseConverter<T> {
 public:
  /// Default Constructor
  ConverterNChannels(int channels) {
    this->channels = channels;
    filters = new Filter<FT> *[channels];
    // make sure that we have 1 filter per channel
    for (int j = 0; j < channels; j++) {
      filters[j] = nullptr;
    }
  }

  /// Destrucotr
  ~ConverterNChannels() {
    for (int j = 0; j < channels; j++) {
      if (filters[j]!=nullptr){
        delete filters[j];
      }
    }
    delete[] filters;
    filters = 0;
  }

  /// defines the filter for an individual channel - the first channel is 0
  void setFilter(int channel, Filter<FT> *filter) {
    if (channel<channels){
      if (filters[channel]!=nullptr){
        delete filters[channel];
      }
      filters[channel] = filter;
    } else {
      LOGE("Invalid channel nummber %d - max channel is %d", channel, channels-1);
    }
  }

  // convert all samples for each channel separately
  size_t convert(uint8_t *src, size_t size) {
    int count = size / channels / sizeof(T);
    T *sample = (T *)src;
    for (size_t j = 0; j < count; j++) {
      for (int channel = 0; channel < channels; channel++) {
        if (filters[channel]!=nullptr){
          *sample = filters[channel]->process(*sample);
        }
        sample++;
      }
    }
    return size;
  }

 protected:
  Filter<FT> **filters = nullptr;
  int channels;
};

}  // namespace audio_tools
