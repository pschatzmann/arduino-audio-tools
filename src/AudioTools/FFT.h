/**
 * Cooley–Tukey FFT inpired by
 * https://rosettacode.org/wiki/Fast_Fourier_transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <complex>
#include <iostream>
#include <valarray>

/**
 * @brief Template Array of complex numbers
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class T> using FFTArray = std::valarray<std::complex<T>>;

/**
 * @brief Cooley–Tukey FFT which allows the speicification of the numeric data
 * type
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class NT> class FFTBase {
public:
  /// forward fft
  virtual FFTArray<NT> &calculateArray(NT array[], int len) {
    FFTArray<NT> complex_array(len);
    for (int j = 0; j < len; ++j) {
      complex_array[j] = array[j];
    }
    calculate(complex_array);
    return complex_array;
  }

  /// forward fft
  virtual FFTArray<NT> &calculateArray(std::valarray<NT> &x) {
    if (complex_array.size() != x.size()) {
      complex_array.resize(x.size());
    }
    for (int j = 0; j < x.size(); j++) {
      complex_array[j] = x[j];
    }
    calculate(complex_array);
    return complex_array;
  }

  /// forward fft (in-place)
  virtual void calculate(FFTArray<NT> &x) = 0;

  /// inverse fft (in-place)
  void invert(FFTArray<NT> &x) {
    // conjugate the complex numbers
    x = x.apply(std::conj);

    // forward fft
    calculate(x);

    // conjugate the complex numbers again
    x = x.apply(std::conj);

    // scale the numbers
    x /= x.size();
  }

protected:
  FFTArray<NT> complex_array;
};

/**
 * @brief  Cooley-Tukey FFT (in-place, breadth-first, decimation-in-frequency)
 * Better optimized but less intuitive
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class NT> class FFT : public FFTBase<NT> {
public:
  /// forward fft (in-place)
  void calculate(FFTArray<NT> &x) override {
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
};

/**
 * @brief Cooley–Tukey FFT (in-place, divide-and-conquer)
 * Higher memory requirements and redundancy although more intuitive
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <class NT> class FFTDivideAndConquer : public FFTBase<NT> {
public:
  /// forward fft (in-place)
  void calculate(FFTArray<NT> &x) override {
    const size_t N = x.size();
    if (N <= 1)
      return;

    // divide
    FFTArray<NT> even = x[std::slice(0, N / 2, 2)];
    FFTArray<NT> odd = x[std::slice(1, N / 2, 2)];

    // conquer
    fft(even);
    fft(odd);

    // combine
    for (size_t k = 0; k < N / 2; ++k) {
      std::complex<NT> t = std::polar(1.0, -2 * PI * k / N) * odd[k];
      x[k] = even[k] + t;
      x[k + N / 2] = even[k] - t;
    }
  }
};