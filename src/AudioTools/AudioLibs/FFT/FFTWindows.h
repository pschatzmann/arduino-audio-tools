/**
 * @file FFTWindows.h
 * @author Phil Schatzmann
 * @brief Different Window functions that can be used by FFT
 * @version 0.1
 * @date 2022-04-29
 *
 * @copyright Copyright (c) 2022
 *
 */
#pragma once

#include <math.h>

namespace audio_tools {

/**
 * @brief FFT Window Function
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class WindowFunction {
 public:
  WindowFunction() = default;

  /// Setup the window function providing the fft length
  virtual void begin(int samples) {
    this->samples_minus_1 = -1.0f + samples;
    this->i_samples = samples;
    this->i_half_samples = samples / 2 - 1;
  }

  /// Provides the multipication factor at the indicated position. The result is symetirically mirrored around the center
  inline float factor(int idx) {
    float result =  idx < i_half_samples ? factor_internal(idx) : factor_internal(i_samples-idx-1);
    return result>1.0f ? 1.0f : result;
  }

  /// Provides the number of samples (fft length)
  inline int samples() { return i_samples; }

 protected:
  float samples_minus_1 = 0.0f;
  int i_samples = 0;
  int i_half_samples = 0;
  const float twoPi = 6.28318531f;
  const float fourPi = 12.56637061f;
  const float sixPi = 18.84955593f;

  // virtual function provide implementation in subclass
  virtual float factor_internal(int idx) = 0;

  // the ratio idx / samples -1
  inline float ratio(int idx) {
    return (static_cast<float>(idx)) / samples_minus_1;
  }

};

/**
 * @brief Buffered window function, so that we do not need to re-calculate the
 * values
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BufferedWindow : public WindowFunction {
 public:
  BufferedWindow(WindowFunction* wf) { p_wf = wf; }
  BufferedWindow(BufferedWindow const&) = delete;
  BufferedWindow& operator=(BufferedWindow const&) = delete;

  virtual void begin(int samples) override {
    // process only if there is a change
    WindowFunction::begin(samples);
    if (p_wf->samples() != samples) {
      p_wf->begin(samples);
      len = samples / 2;
      buffer.resize(len);
      for (int j = 0; j < len; j++) {
        buffer[j] = p_wf->factor(j);
      }
    }
  }

 protected:
  WindowFunction* p_wf = nullptr;
  Vector<float> buffer{0};
  int len;

  float factor_internal(int idx) override {
    return idx < len ? buffer[idx] : buffer[i_samples - idx];
  }
};

/**
 * @brief Rectange FFT Window function
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Rectange : public WindowFunction {
 public:
  Rectange() = default;
  float factor_internal(int idx) { return 1.0f; }
};

/**
 * @brief Hamming FFT Window function
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Hamming : public WindowFunction {
 public:
  Hamming() = default;
  float factor_internal(int idx) {
    return 0.54f - (0.46f * cos(twoPi * ratio(idx)));
  }
};

/**
 * @brief Hann FFT Window function
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Hann : public WindowFunction {
 public:
  Hann() = default;
  float factor_internal(int idx) {
    return 0.54f * (1.0f - cos(twoPi * ratio(idx)));
  }
};

/**
 * @brief Triangle FFT Window function
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Triangle : public WindowFunction {
 public:
  Triangle() = default;
  float factor_internal(int idx) {
    return 1.0f - ((2.0f * fabs((idx - 1) -
                              (static_cast<float>(i_samples - 1) / 2.0f))) /
                  samples_minus_1);
  }
};

/**
 * @brief Nuttall FFT Window function
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class Nuttall : public WindowFunction {
 public:
  Nuttall() = default;
  float factor_internal(int idx) override {
    float r = ratio(idx);
    return 0.355768f - (0.487396f * (cos(twoPi * r))) +
           (0.144232f * (cos(fourPi * r))) - (0.012604f * (cos(sixPi * r)));
  }
};

/**
 * @brief Blackman FFT Window function
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class Blackman : public WindowFunction {
 public:
  Blackman() = default;
  float factor_internal(int idx)override {
    float r = ratio(idx);
    return 0.42323f - (0.49755f * (cos(twoPi * r))) +
           (0.07922f * (cos(fourPi * r)));
  }
};

/**
 * @brief BlackmanNuttall FFT Window function
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BlackmanNuttall : public WindowFunction {
 public:
  BlackmanNuttall() = default;
  float factor_internal(int idx) override{
    float r = ratio(idx);
    return 0.3635819f - (0.4891775f * (cos(twoPi * r))) +
           (0.1365995f * (cos(fourPi * r))) - (0.0106411f * (cos(sixPi * r)));
  }
};

/**
 * @brief BlackmanHarris FFT Window function
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class BlackmanHarris : public WindowFunction {
 public:
  BlackmanHarris() = default;
  float factor_internal(int idx) override{
    float r = ratio(idx);
    return 0.35875f - (0.48829f * (cos(twoPi * r))) +
           (0.14128f * (cos(fourPi * r))) - (0.01168f * (cos(sixPi * r)));
  }
};

/**
 * @brief FlatTop FFT Window function
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FlatTop : public WindowFunction {
 public:
  FlatTop() = default;
  float factor_internal(int idx) override{
    float r = ratio(idx);
    return 0.2810639f - (0.5208972f * cos(twoPi * r)) +
           (0.1980399f * cos(fourPi * r));
  }
};

/**
 * @brief Welch FFT Window function
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Welch : public WindowFunction {
 public:
  Welch() = default;
  float factor_internal(int idx) override{
    float tmp = (((idx - 1) - samples_minus_1 / 2.0f) / (samples_minus_1 / 2.0f));
    return 1.0f - (tmp*tmp);
  }
};

}  // namespace audio_tools