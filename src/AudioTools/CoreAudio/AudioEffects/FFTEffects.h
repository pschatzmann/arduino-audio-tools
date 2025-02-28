#pragma once

#include "AudioTools/AudioLibs/AudioRealFFT.h"  // using RealFFT
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/StreamCopy.h"

namespace audio_tools {

static Hann fft_effects_hann;
static BufferedWindow fft_effects_buffered_window{&fft_effects_hann};

/**
 * @brief Common configuration for FFT effects
 * @ingroup transform
 * @author phil schatzmann
 */
struct FFTEffectConfig : public AudioInfo {
  int length = 1024;
  int stride = 512;
  WindowFunction *window_function = &fft_effects_buffered_window;
};

/***
 * @brief Abstract class for common Logic for FFT based effects. The effect is
 * applied after the fft to the frequency domain before executing the ifft.
 * Please note that this is quite processing time intensitive: so you might keep
 * the sample rate quite low if the processor is not fast enough!
 * @ingroup transform
 * @author phil schatzmann
 */

class FFTEffect : public AudioOutput {
 public:
  FFTEffect(Print &out) {
    p_out = &out;
    fft_cfg.ref = this;
  }

  FFTEffectConfig defaultConfig() {
    FFTEffectConfig c;
    return c;
  }

  bool begin(FFTEffectConfig info) {
    copier.setLogName("ifft");
    setAudioInfo(info);
    fft_cfg.length = info.length;
    fft_cfg.stride = info.stride > 0 ? info.stride : info.length;
    fft_cfg.window_function = info.window_function;
    return begin();
  }

  bool begin() override {
    TRACED();
    // copy result to output
    copier.begin(*p_out, fft);

    // setup fft
    fft_cfg.copyFrom(audioInfo());
    fft_cfg.callback = effect_callback;
    LOGI("length: %d", fft_cfg.length);
    LOGI("stride: %d", fft_cfg.stride);
    LOGI("window_function: %s", (fft_cfg.window_function != nullptr)
                                    ? fft_cfg.window_function->name()
                                    : "-");
    return fft.begin(fft_cfg);
  }

  size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    return fft.write(data, len);
  }

 protected:
  Print *p_out = nullptr;
  AudioRealFFT fft;
  AudioFFTConfig fft_cfg{fft.defaultConfig(RXTX_MODE)};
  Hann hann;
  BufferedWindow buffered{&hann};
  StreamCopy copier;

  virtual void effect(AudioFFTBase &fft) = 0;

  static void effect_callback(AudioFFTBase &fft) {
    TRACED();
    FFTEffect *ref = (FFTEffect *)fft.config().ref;
    // execute effect
    ref->effect(fft);
    // write ifft to output
    ref->processOutput();
  }

  void processOutput() {
    TRACED();
    while (copier.copy());
  }
};

/**
 * @brief Apply Robotize FFT Effect on frequency domain data. See
 * https://learn.bela.io/tutorials/c-plus-plus-for-real-time-audio-programming/phase-vocoder-part-3/
 * @ingroup transform
 * @author phil schatzmann
 */
class FFTRobotize : public FFTEffect {
  friend FFTEffect;

 public:
  FFTRobotize(AudioStream &out) : FFTEffect(out) { addNotifyAudioChange(out); };
  FFTRobotize(AudioOutput &out) : FFTEffect(out) { addNotifyAudioChange(out); };
  FFTRobotize(Print &out) : FFTEffect(out) {};

 protected:
  /// Robotise the output
  void effect(AudioFFTBase &fft) {
    TRACED();
    AudioFFTResult best = fft.result();

    FFTBin bin;
    for (int n = 0; n < fft.size(); n++) {
      float amplitude = fft.magnitude(n);

      // update new bin value
      bin.real = amplitude / best.magnitude;
      bin.img = 0.0;
      Serial.println(bin.real);

      fft.setBin(n, bin);
    }
  }
};

/**
 * @brief Apply Robotize FFT Effect on frequency domain data. See
 * https://learn.bela.io/tutorials/c-plus-plus-for-real-time-audio-programming/phase-vocoder-part-3/
 * @ingroup transform
 * @author phil schatzmann
 */
class FFTWhisper : public FFTEffect {
  friend FFTEffect;

 public:
  FFTWhisper(AudioStream &out) : FFTEffect(out) { addNotifyAudioChange(out); };
  FFTWhisper(AudioOutput &out) : FFTEffect(out) { addNotifyAudioChange(out); };
  FFTWhisper(Print &out) : FFTEffect(out) {};

 protected:
  /// Robotise the output
  void effect(AudioFFTBase &fft) {
    TRACED();
    FFTBin bin;
    for (int n = 0; n < fft.size(); n++) {
      float amplitude = fft.magnitude(n);
      float phase = rand() / (float)RAND_MAX * 2.f * PI;

      // update new bin value
      bin.real = cosf(phase) * amplitude;
      bin.img = sinf(phase) * amplitude;
      fft.setBin(n, bin);
    }
  }
};

/**
 * @brief Apply FFT and IFFT w/o any changes to the frequency domain
 * @ingroup transform
 * @author phil schatzmann
 */

class FFTNop : public FFTEffect {
  friend FFTEffect;

 public:
  FFTNop(AudioStream &out) : FFTEffect(out) { addNotifyAudioChange(out); };
  FFTNop(AudioOutput &out) : FFTEffect(out) { addNotifyAudioChange(out); };
  FFTNop(Print &out) : FFTEffect(out) {};

 protected:
  /// Do nothing
  void effect(AudioFFTBase &fft) {}
};

/**
 * @brief  Pitch Shift FFT Effect Configuration
 * @ingroup transform
 * @author phil schatzmann
 */

struct FFTPitchShiftConfig : public FFTEffectConfig {
  int shift = 1;
};

/**
 * @brief Apply Pitch Shift FFT Effect on frequency domain data: we just move
 * the bins up or down
 * @ingroup transform
 * @author phil schatzmann
 */
class FFTPitchShift : public FFTEffect {
  friend FFTEffect;

 public:
  FFTPitchShift(AudioStream &out) : FFTEffect(out) {
    addNotifyAudioChange(out);
  };
  FFTPitchShift(AudioOutput &out) : FFTEffect(out) {
    addNotifyAudioChange(out);
  };
  FFTPitchShift(Print &out) : FFTEffect(out) {};

  FFTPitchShiftConfig defaultConfig() {
    FFTPitchShiftConfig result;
    result.shift = shift;
    return result;
  }

  bool begin(FFTPitchShiftConfig psConfig) {
    setShift(psConfig.shift);
    FFTEffect::begin(psConfig);
    return begin();
  }

  bool begin() override {
    bool rc = FFTEffect::begin();
    // you can not shift more then you have bins
    assert(abs(shift) < fft.size());
    return rc;
  }

  /// defines how many bins should be shifted up (>0) or down (<0);
  void setShift(int bins) { shift = bins; }

 protected:
  int shift = 1;

  /// Pitch Shift
  void effect(AudioFFTBase &fft) {
    TRACED();
    FFTBin bin;
    int max = fft.size();

    if (shift < 0) {
      // copy bins: left shift
      for (int n = -shift; n < max; n++) {
        int to_bin = n + shift;
        assert(to_bin >= 0);
        assert(to_bin < max);
        fft.getBin(n, bin);
        fft.setBin(to_bin, bin);
      }
      // clear tail
      bin.clear();
      for (int n = max + shift; n < max; n++) {
        fft.setBin(n, bin);
      }
    } else if (shift > 0) {
      // copy bins: right shift
      for (int n = max - shift - 1; n >= 0; n--) {
        int to_bin = n + shift;
        assert(to_bin >= 0);
        assert(to_bin < max);
        fft.getBin(n, bin);
        fft.setBin(to_bin, bin);
      }
      // clear head
      bin.clear();
      for (int n = 0; n < shift; n++) {
        fft.setBin(n, bin);
      }
    }
  }
};

}  // namespace audio_tools