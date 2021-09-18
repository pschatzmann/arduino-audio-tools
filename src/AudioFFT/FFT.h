#pragma once
#include "AudioConfig.h"
#include <complex>
#include <iostream>
#include <valarray>
#include <vector>

namespace audio_tools {

enum WindowFunction {None, Hanning, Hamming, Triangular, Gauss, BlackmanHarris, Random};

/**
 * @brief Support for different FFT Window functions: Hanning, Hamming, Triangular, Gauss, BlackmanHarris, Random
 * 
 */
class WindowCalculator {
  public:
    float value(int point, int numSamples, WindowFunction func){
      switch(func){
        case Hanning:
          return hanning(point, numSamples);
        case Hamming: 
          return hamming(point, numSamples);
        case Triangular: 
          return triangular(point, numSamples);
        case Gauss:
          return gauss(point, numSamples);
        case BlackmanHarris:
          return blackmanHarris(point, numSamples);
        case Random:
          return hanning(point, numSamples);
        default:
          return 1.0;
      }
    }
  protected:

    float hanning(float point, int numSamples){
      return 0.5f * ( 1.0f - cosf ( (2.0f * M_PI * point) / ( (float)numSamples - 1.0f) ) );
    }

    float hamming(int point, int numSamples){
      return 0.54f - 0.46f * cosf ( (2.0f * M_PI * point) / ( (float)numSamples - 1.0f) ) ;
    }

    float triangular(int point, int numSamples){
      return ( 2.0f / numSamples ) * ( ( numSamples * 0.5f ) - fabs( point - ( numSamples -1.0f ) * 0.5f ) );
    }

    float gauss(int point, int numSamples){
      float bellW = 0.4f;
      return powf ( M_E, -0.5f * powf ( ( point - ( numSamples - 1 ) * 0.5f ) / ( bellW * ( numSamples - 1 ) * 0.5f ) , 2.0f ) );
    }

    float blackmanHarris(int point, int numSamples){
      return 0.35875f		- 0.48829f * cosf( 2.0f * M_PI * point / (numSamples-1) ) 
                + 0.14128f * cosf( 4.0f * M_PI * point / (numSamples-1) ) 
                - 0.01168f * cosf( 6.0f * M_PI * point / (numSamples-1) );
    }

    float random(int point, int numSamples){
      return rand()%1000 / 1000.0f;
    }
};


#if FFT_IMPL == CUSTOM_FFT
/**
 * @brief Template Array of complex numbers
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class T> using FFTArray = std::valarray<std::complex<T>>;

#endif

#if FFT_IMPL == POCKET_FFT
/**
 * @brief Template Array of complex numbers
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <class T> using FFTArray = std::vector<std::complex<T>>;

#endif


/**
 * @brief Cooley–Tukey FFT which allows the specification of the numeric data type NT
 * type
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class NT> class FFTBase {
public:
  /// forward fft
  virtual FFTArray<NT> &calculateArray(NT array[], int len, WindowFunction func) {
    if (complex_array.size() != len) {
      complex_array.resize(len);
    }
    for (int j = 0; j < len; ++j) {
      complex_array[j] = array[j];
    }    
    calculate(complex_array, func);
    return complex_array;
  }

  /// forward fft
  virtual FFTArray<NT> &calculateArray(std::valarray<NT> &x, WindowFunction func) {
    if (complex_array.size() != x.size()) {
      complex_array.resize(x.size());
    }
    for (int j = 0; j < x.size(); j++) {
      complex_array[j] = x[j];
    }
    calculate(complex_array, func);
    return complex_array;
  }

  /// forward fft (in-place)
  virtual void calculate(FFTArray<NT> &data, WindowFunction wf) = 0;

  /// revert fft (in-place)
  virtual void invert(FFTArray<NT> &x) = 0;

protected:
  FFTArray<NT> complex_array;
  WindowCalculator windowCalculator;

  virtual void applyWindow(FFTArray<NT> &data, WindowFunction wf) {
    // adjust values with window function
    int n = data.size();
    for (int j=0;j<n;j++){
      float factor = windowCalculator.value(j, n, wf);
      std::complex<double> updated (data[j].real() * factor, data[j].imag() *factor);
      data[j] = updated;
    }
  }

};

#if FFT_IMPL == CUSTOM_FFT

/**
 * @brief  Cooley-Tukey FFT (in-place, breadth-first, decimation-in-frequency)
 * Better optimized but less intuitive
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class NT> class FFT : public FFTBase<NT> {
public:
  /// forward fft (in-place)
  void calculate(FFTArray<NT> &data, WindowFunction wf) override {
    this->applyWindow(data, wf);

    // DFT
    unsigned int N = x.size(), k = N, n;
    NT thetaT = 3.14159265358979323846264338328L / N;
    std::complex<NT> phiT = std::complex<NT>(cos(thetaT), -sin(thetaT)), T;
    while (k > 1) {
      n = k;
      k >>= 1;
      phiT = phiT * phiT;
      T = 1.0L;
      for (unsigned int l = 0; l < k; l++) {
        for (unsigned int a = l; a < N; a += n) {
          unsigned int b = a + k;
          std::complex<NT> t = x[a] - x[b];
          x[a] += x[b];
          x[b] = t * T;
        }
        T *= phiT;
      }
    }
    // Decimate
    unsigned int m = (unsigned int)log2(N);
    for (unsigned int a = 0; a < N; a++) {
      unsigned int b = a;
      // Reverse bits
      b = (((b & 0xaaaaaaaa) >> 1) | ((b & 0x55555555) << 1));
      b = (((b & 0xcccccccc) >> 2) | ((b & 0x33333333) << 2));
      b = (((b & 0xf0f0f0f0) >> 4) | ((b & 0x0f0f0f0f) << 4));
      b = (((b & 0xff00ff00) >> 8) | ((b & 0x00ff00ff) << 8));
      b = ((b >> 16) | (b << 16)) >> (32 - m);
      if (b > a) {
        std::complex<NT> t = x[a];
        x[a] = x[b];
        x[b] = t;
      }
    }
  }

  /// inverse fft (in-place)
  virtual void invert(FFTArray<NT> &x) {
    // conjugate the complex numbers
    x = x.apply(std::conj);

    // forward fft
    calculate(x);

    // conjugate the complex numbers again
    x = x.apply(std::conj);

    // scale the numbers
    x /= x.size();
  }

};

#endif



#if FFT_IMPL == POCKET_FFT

#include <cmath>
#include "AudioFFT/PocketFFT.h"

/**
 * @brief FFT API using Pocket FFT
 * 
 * @tparam NT 
 */
template <class NT> class FFT : public FFTBase<NT> {
public:
  /**
   * @brief Forward (in place) FFT using pocketfft
   * 
   * @param data 
   */
  void calculate(FFTArray<NT> &data, WindowFunction wf) override {
    const size_t N = data.size();
    // adjust values with window function
    this->applyWindow(data, wf);
    pocketfft::shape_t axes = {0};
    pocketfft::shape_t shape{N};
    pocketfft::stride_t stridef(shape.size()), strided(shape.size()), stridel(shape.size());
    pocketfft::c2c<NT>(shape, stridel, stridel, axes, true, data.data(), data.data(), 1.L);
  }

  /**
   * @brief Revert FFT (in place)
   * 
   */
  void invert(FFTArray<NT>&data) override {
    const size_t N = data.size();
    pocketfft::shape_t axes = {0};
    pocketfft::shape_t shape{N};
    pocketfft::stride_t stridef(shape.size()), strided(shape.size()), stridel(shape.size());
    pocketfft::c2c<NT>(shape, stridel, stridel, axes, false, data.data(), data.data(), 1.L);
  }


};

#endif

}