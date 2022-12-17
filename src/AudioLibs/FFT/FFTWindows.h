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

  virtual void begin(int samples) {
    this->samples_minus_1 = -1.0f + samples;
    this->i_samples = samples;
  }

  inline float ratio(int idx) {
    float result = (static_cast<float>(idx)) / samples_minus_1;
    return result>1.0 ? 1.0 : result;
  }

  inline int samples() { return i_samples; }
  virtual float factor(int idx) = 0;

 protected:
  float samples_minus_1 = 0;
  int i_samples = 0;
  const float twoPi = 6.28318531;
  const float fourPi = 12.56637061;
  const float sixPi = 18.84955593;
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


  virtual void begin(int samples) {
    // process only if there is a change
    if (p_wf->samples() != samples) {
      p_wf->begin(samples);
      len = samples / 2;
      if (p_buffer != nullptr) delete[] p_buffer;
      p_buffer = new float[len];
      for (int j = 0; j < len; j++) {
        p_buffer[j] = p_wf->factor(j);
      }
    }
  }

  ~BufferedWindow() {
    if (p_buffer != nullptr) delete[] p_buffer;
  }

  inline float factor(int idx) {
    return idx < len ? p_buffer[idx] : p_buffer[i_samples - idx];
  }

 protected:
  WindowFunction* p_wf = nullptr;
  float* p_buffer = nullptr;
  int len;
};

/**
 * @brief Rectange FFT Window function
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Rectange : public WindowFunction {
 public:
  Rectange() = default;
  float factor(int idx) { return 1.0; }
};

/**
 * @brief Hamming FFT Window function
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Hamming : public WindowFunction {
 public:
  Hamming() = default;
  float factor(int idx) {
    return 0.54 - (0.46 * cos(twoPi * ratio(idx)));
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
  float factor(int idx) {
    return 0.54 * (1.0 - cos(twoPi * ratio(idx)));
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
  float factor(int idx) {
    return 1.0 - ((2.0 * fabs((idx - 1) -
                              (static_cast<float>(i_samples - 1) / 2.0))) /
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
  float factor(int idx) {
    float r = ratio(idx);
    return 0.355768 - (0.487396 * (cos(twoPi * r))) +
           (0.144232 * (cos(fourPi * r))) - (0.012604 * (cos(sixPi * r)));
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
  float factor(int idx) {
    float r = ratio(idx);
    return 0.42323 - (0.49755 * (cos(twoPi * r))) +
           (0.07922 * (cos(fourPi * r)));
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
  float factor(int idx) {
    float r = ratio(idx);
    return 0.3635819 - (0.4891775 * (cos(twoPi * r))) +
           (0.1365995 * (cos(fourPi * r))) - (0.0106411 * (cos(sixPi * r)));
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
  float factor(int idx) {
    float r = ratio(idx);
    return 0.35875 - (0.48829 * (cos(twoPi * r))) +
           (0.14128 * (cos(fourPi * r))) - (0.01168 * (cos(sixPi * r)));
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
  float factor(int idx) {
    float r = ratio(idx);
    return 0.2810639 - (0.5208972 * cos(twoPi * r)) +
           (0.1980399 * cos(fourPi * r));
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
  float factor(int idx) {
    float tmp = (((idx - 1) - samples_minus_1 / 2.0) / (samples_minus_1 / 2.0));
    return 1.0 - (tmp*tmp);
  }
};

}  // namespace audio_tools