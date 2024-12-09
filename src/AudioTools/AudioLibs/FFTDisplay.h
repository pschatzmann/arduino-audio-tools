#pragma once
#include "AudioTools/AudioLibs/AudioFFT.h"
#include "AudioTools/Concurrency/LockGuard.h"

namespace audio_tools {

class FFTDisplay;
static FFTDisplay *selfFFTDisplay = nullptr;
#if defined(USE_CONCURRENCY)
// fft mutex
static Mutex fft_mux;
#endif
/**
 * Display FFT result: we can define a start bin and group susequent bins for a
 * combined result.
 */

class FFTDisplay {
 public:
  FFTDisplay(AudioFFTBase &fft) {
    p_fft = &fft;
    selfFFTDisplay = this;
  }

  /// start bin which is displayed
  int fft_start_bin = 0;
  /// group result by adding subsequent bins
  int fft_group_bin = 1;
  /// Influences the senitivity
  float fft_max_magnitude = 700.0f;

  void begin() {
    // assign fft callback
    AudioFFTConfig &fft_cfg = p_fft->config();
    fft_cfg.callback = fftCallback;

    // number of bins
    magnitudes.resize(p_fft->size());
    for (int j = 0; j < p_fft->size(); j++) {
      magnitudes[j] = 0;
    }
  }

  /// Returns the magnitude for the indicated led x position. We might
  /// need to combine values from the magnitudes array if this is much bigger.
  float getMagnitude(int x) {
    // get magnitude from fft
    float total = 0;
    for (int j = 0; j < fft_group_bin; j++) {
      int idx = fft_start_bin + (x * fft_group_bin) + j;
      if (idx >= magnitudes.size()) {
        idx = magnitudes.size() - 1;
      }
      total += magnitudes[idx];
    }
    return total / fft_group_bin;
  }

  int getMagnitudeScaled(int x, int max) {
    int result = mapT<float>(getMagnitude(x), 0, fft_max_magnitude, 0.0f,
                    static_cast<float>(max));
    if (result > max){
      LOGD("fft_max_magnitude too small: current value is %f", getMagnitude(x))
    }
    // limit value to max                
    return min(result, max); 
  }

  /// callback method which provides updated data from fft
  static void fftCallback(AudioFFTBase &fft) {
    selfFFTDisplay->loadMangnitudes();
  };

 protected:
  AudioFFTBase *p_fft = nullptr;
  Vector<float> magnitudes{0};

  void loadMangnitudes() {
    // just save magnitudes to be displayed
#if defined(USE_CONCURRENCY)
    LockGuard guard(fft_mux);
#endif
    for (int j = 0; j < p_fft->size(); j++) {
      float value = p_fft->magnitude(j);
      magnitudes[j] = value;
    }
  }
};

}  // namespace audio_tools
