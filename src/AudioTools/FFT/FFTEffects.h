#pragma once

#include "AudioTools/FFT/AudioRealFFT.h"  // using RealFFT
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

/**
 * @brief Abstract base class for effects that operate on audio in the
 * frequency domain: incoming samples are transformed with a forward FFT, the
 * resulting bins are modified by the effect() method that a subclass
 * implements, and the modified spectrum is transformed back with an inverse
 * FFT (overlap-add) and streamed to the output.
 *
 * Processing pipeline driven by write():
 *  -# incoming samples are accumulated into windows of
 *     FFTEffectConfig::length samples, hopping by FFTEffectConfig::stride
 *     (stride < length gives overlapping windows)
 *  -# a windowed forward FFT is calculated once a window is complete
 *  -# effect(AudioFFTBase&) is invoked so the subclass can inspect/replace
 *     bins via fft.magnitude()/fft.getBin()/fft.setBin()
 *  -# the inverse FFT is calculated and the result is overlap-added and
 *     copied to the output
 *
 * Usage:
 * @code
 * FFTPitchShift effect(out);  // or FFTRobotize, FFTWhisper, FFTNop, FFTEqualizer...
 * auto cfg = effect.defaultConfig();
 * cfg.copyFrom(info);  // sample_rate / channels / bits_per_sample
 * cfg.length = 1024;   // fft size, must be a power of two
 * cfg.stride = 512;    // hop size; < length gives overlap-add
 * effect.begin(cfg);
 * @endcode
 *
 * By default an internally owned AudioRealFFT performs the fft/ifft; pass a
 * different AudioFFTBase-derived instance (e.g. AudioKissFFT, AudioCmsisFFT,
 * AudioESP32FFT, AudioEspressifFFT, AudioFixedFFT) to the constructor to use
 * a different implementation. The engine must outlive the effect.
 *
 * To implement a new effect, subclass FFTEffect and override
 * effect(AudioFFTBase&); see FFTRobotize, FFTWhisper, FFTPitchShift and
 * FFTEqualizer for examples.
 *
 * @note This is quite processing time intensive: keep the sample rate low
 * if the processor is not fast enough to keep up in real time or select an
 * integer based FFT implementation (AudioFixedFFT) instead of the default AudioRealFFT.
 * @note Effectively mono only: the forward FFT analyzes just
 * AudioFFTConfig::channel_used (default channel 0) and discards the other
 * channels' input samples; the reconstructed signal is then written
 * identically to every output channel. With stereo input/output this
 * collapses the signal to dual-mono and loses the original stereo image -
 * it does not process channels independently.
 * @ingroup transform
 * @ingroup fft
 * @author phil schatzmann
 */

class FFTEffect : public AudioOutput {
 public:
  FFTEffect(Print &out) {
    p_out = &out;
    p_fft = &default_fft;
    fft_cfg.ref = this;
  }

  FFTEffect(Print &out, AudioFFTBase &fft) {
    p_out = &out;
    p_fft = &fft;
    fft_cfg.ref = this;
  }

  FFTEffectConfig defaultConfig() {
    FFTEffectConfig c;
    // use a window function owned by this instance: sharing the static
    // default across multiple concurrently active effects would let one
    // instance's begin() resize the buffers out from under another
    c.window_function = &buffered;
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
    copier.begin(*p_out, *p_fft);

    // setup fft
    fft_cfg.copyFrom(audioInfo());
    fft_cfg.callback = effect_callback;
    LOGI("length: %d", fft_cfg.length);
    LOGI("stride: %d", fft_cfg.stride);
    LOGI("window_function: %s", (fft_cfg.window_function != nullptr)
                                    ? fft_cfg.window_function->name()
                                    : "-");
    return p_fft->begin(fft_cfg);
  }

  size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    return p_fft->write(data, len);
  }

 protected:
  Print *p_out = nullptr;
  AudioRealFFT default_fft;
  AudioFFTBase *p_fft = nullptr;
  AudioFFTConfig fft_cfg{default_fft.defaultConfig(RXTX_MODE)};
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
 public:
  FFTRobotize(AudioStream &out) : FFTEffect(out) { addNotifyAudioChange(out); };
  FFTRobotize(AudioStream &out, AudioFFTBase &fft) : FFTEffect(out, fft) { addNotifyAudioChange(out); };
  FFTRobotize(AudioOutput &out) : FFTEffect(out) { addNotifyAudioChange(out); };
  FFTRobotize(AudioOutput &out, AudioFFTBase &fft) : FFTEffect(out, fft) { addNotifyAudioChange(out); };
  FFTRobotize(Print &out) : FFTEffect(out) {};
  FFTRobotize(Print &out, AudioFFTBase &fft) : FFTEffect(out, fft) {};

 protected:
  /// Robotise the output: keep the magnitude of each bin but zero out the
  /// phase, which collapses all partials into a fixed phase relationship
  void effect(AudioFFTBase &fft) override {
    TRACED();
    FFTBin bin;
    for (int n = 0; n < fft.size(); n++) {
      // update new bin value
      bin.real = fft.magnitude(n);
      bin.img = 0.0;

      fft.setBin(n, bin);
    }
  }
};

/**
 * @brief Apply Whisper FFT Effect on frequency domain data: keep the
 * magnitude of each bin but randomize its phase. See
 * https://learn.bela.io/tutorials/c-plus-plus-for-real-time-audio-programming/phase-vocoder-part-3/
 * @ingroup transform
 * @author phil schatzmann
 */
class FFTWhisper : public FFTEffect {
 public:
  FFTWhisper(AudioStream &out) : FFTEffect(out) { addNotifyAudioChange(out); };
  FFTWhisper(AudioStream &out, AudioFFTBase &fft) : FFTEffect(out, fft) { addNotifyAudioChange(out); };
  FFTWhisper(AudioOutput &out) : FFTEffect(out) { addNotifyAudioChange(out); };
  FFTWhisper(AudioOutput &out, AudioFFTBase &fft) : FFTEffect(out, fft) { addNotifyAudioChange(out); };
  FFTWhisper(Print &out) : FFTEffect(out) {};
  FFTWhisper(Print &out, AudioFFTBase &fft) : FFTEffect(out, fft) {};

 protected:
  /// Whisperize the output
  void effect(AudioFFTBase &fft) override {
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
 public:
  FFTNop(AudioStream &out) : FFTEffect(out) { addNotifyAudioChange(out); };
  FFTNop(AudioStream &out, AudioFFTBase &fft) : FFTEffect(out, fft) { addNotifyAudioChange(out); };
  FFTNop(AudioOutput &out) : FFTEffect(out) { addNotifyAudioChange(out); };
  FFTNop(AudioOutput &out, AudioFFTBase &fft) : FFTEffect(out, fft) { addNotifyAudioChange(out); };
  FFTNop(Print &out) : FFTEffect(out) {};
  FFTNop(Print &out, AudioFFTBase &fft) : FFTEffect(out, fft) {};

 protected:
  /// Do nothing
  void effect(AudioFFTBase &fft) override {}
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
 public:
  FFTPitchShift(AudioStream &out) : FFTEffect(out) {
    addNotifyAudioChange(out);
  };
  FFTPitchShift(AudioStream &out, AudioFFTBase &fft) : FFTEffect(out, fft) {
    addNotifyAudioChange(out);
  };
  FFTPitchShift(AudioOutput &out) : FFTEffect(out) {
    addNotifyAudioChange(out);
  };
  FFTPitchShift(AudioOutput &out, AudioFFTBase &fft) : FFTEffect(out, fft) {
    addNotifyAudioChange(out);
  };
  FFTPitchShift(Print &out) : FFTEffect(out) {};
  FFTPitchShift(Print &out, AudioFFTBase &fft) : FFTEffect(out, fft) {};

  FFTPitchShiftConfig defaultConfig() {
    FFTPitchShiftConfig result;
    result.shift = shift;
    result.window_function = &buffered;
    return result;
  }

  bool begin(FFTPitchShiftConfig psConfig) {
    setShift(psConfig.shift);
    // FFTEffect::begin(FFTEffectConfig) internally calls the virtual
    // begin(), which dispatches to begin() below - calling it again here
    // would re-initialize the fft and copier a second time
    return FFTEffect::begin(psConfig);
  }

  bool begin() override {
    bool rc = FFTEffect::begin();
    // you can not shift more then you have bins
    assert(abs(shift) < p_fft->size());
    return rc;
  }

  /// defines how many bins should be shifted up (>0) or down (<0);
  void setShift(int bins) { shift = bins; }

 protected:
  int shift = 1;

  /// Pitch Shift
  void effect(AudioFFTBase &fft) override {
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

/**
 * @brief  Equalizer FFT Effect Configuration
 * @ingroup transform
 * @author phil schatzmann
 */
struct FFTEqualizerConfig : public FFTEffectConfig {
  /// number of logarithmically spaced bands between 20Hz and Nyquist
  int bands = 10;
};

/**
 * @brief Graphic Equalizer implemented as an FFT effect: boosts or cuts
 * logarithmically spaced frequency bands (20Hz .. Nyquist) by scaling the
 * magnitude of the bins that fall into each band while leaving their phase
 * untouched. Since this operates directly on the frequency-domain bins
 * rather than an FIR/IIR approximation, band edges are exact and there is
 * no filter ripple.
 * @ingroup transform
 * @author phil schatzmann
 */
class FFTEqualizer : public FFTEffect {
 public:
  FFTEqualizer(AudioStream &out) : FFTEffect(out) { addNotifyAudioChange(out); };
  FFTEqualizer(AudioStream &out, AudioFFTBase &fft) : FFTEffect(out, fft) { addNotifyAudioChange(out); };
  FFTEqualizer(AudioOutput &out) : FFTEffect(out) { addNotifyAudioChange(out); };
  FFTEqualizer(AudioOutput &out, AudioFFTBase &fft) : FFTEffect(out, fft) { addNotifyAudioChange(out); };
  FFTEqualizer(Print &out) : FFTEffect(out) {};
  FFTEqualizer(Print &out, AudioFFTBase &fft) : FFTEffect(out, fft) {};

  FFTEqualizerConfig defaultConfig() {
    FFTEqualizerConfig c;
    c.window_function = &buffered;
    c.bands = band_count;
    return c;
  }

  bool begin(FFTEqualizerConfig cfg) {
    setBandCount(cfg.bands);
    return FFTEffect::begin(cfg);
  }

  bool begin() override {
    bool rc = FFTEffect::begin();
    setupBands();
    for (int b = 0; b < band_count; b++) {
      LOGI("Band %d: Freq=%.2fHz, Gain=%.2fdB", b, getBandFrequency(b),
           getBandDB(b));
    }
    return rc;
  }

  /// Defines the number of logarithmically spaced bands; call before begin()
  /// or before defaultConfig() if you want a non-default band count.
  void setBandCount(int bands) {
    band_count = bands > 0 ? bands : 1;
    gains_db.resize(band_count);
    gains_linear.resize(band_count);
    band_center_freq.resize(band_count);
    band_bin_from.resize(band_count);
    band_bin_to.resize(band_count);
    for (int b = 0; b < band_count; b++) {
      gains_db[b] = 0.0f;
      gains_linear[b] = 1.0f;
    }
  }

  /// Number of active bands
  int getBandCount() { return band_count; }

  /// Sets the gain for a band directly in dB (-90 to +12 is a sane range;
  /// values are not clamped)
  bool setBandDB(int band, float gain_db) {
    if (band < 0 || band >= band_count) return false;
    gains_db[band] = gain_db;
    gains_linear[band] = powf(10.0f, gain_db / 20.0f);
    return true;
  }

  /// Sets the gain for a band as a normalized volume (-1.0 to 1.0), mapped
  /// to -90dB..+12dB. For deeper cuts/boosts use setBandDB() directly.
  bool setBandGain(int band, float volume) {
    float gain_db = volume < 0 ? volume * 90.0f : volume * 12.0f;
    return setBandDB(band, gain_db);
  }

  /// Sets the same gain for all bands
  bool setBandGains(float volume) {
    bool ok = true;
    for (int b = 0; b < band_count; b++) ok = setBandGain(b, volume) && ok;
    return ok;
  }

  /// Gets the currently defined gain for a band in dB
  float getBandDB(int band) {
    if (band < 0 || band >= band_count) return 0.0f;
    return gains_db[band];
  }

  /// Gets the center frequency of a band in Hz (only valid after begin())
  float getBandFrequency(int band) {
    if (band < 0 || band >= band_count) return 0.0f;
    return band_center_freq[band];
  }

 protected:
  int band_count = 10;
  Vector<float> gains_db;
  Vector<float> gains_linear;
  Vector<float> band_center_freq;
  Vector<int> band_bin_from;
  Vector<int> band_bin_to;

  /// Determines the fft bin range and center frequency of each
  /// logarithmically spaced band between 20Hz and Nyquist
  void setupBands() {
    float nyquist = audioInfo().sample_rate / 2.0f;
    float f_min = log10f(20.0f);
    float f_max = log10f(nyquist);
    float step = (f_max - f_min) / band_count;
    int prev_bin = 0;
    for (int b = 0; b < band_count; b++) {
      // last band always reaches the last bin, regardless of rounding
      int edge_bin = (b == band_count - 1)
                          ? p_fft->size()
                          : p_fft->frequencyToBin(
                                (int)powf(10.0f, f_min + step * (b + 1)));
      band_bin_from[b] = prev_bin;
      band_bin_to[b] = edge_bin;
      band_center_freq[b] = powf(10.0f, f_min + step * (b + 0.5f));
      prev_bin = edge_bin;
    }
  }

  /// Scales the magnitude of the bins in each band by the configured gain,
  /// leaving the phase untouched
  void effect(AudioFFTBase &fft) override {
    TRACED();
    FFTBin bin;
    for (int b = 0; b < band_count; b++) {
      float gain = gains_linear[b];
      if (gain == 1.0f) continue;  // 0dB: bin is already correct
      for (int n = band_bin_from[b]; n < band_bin_to[b]; n++) {
        fft.getBin(n, bin);
        bin.real *= gain;
        bin.img *= gain;
        fft.setBin(n, bin);
      }
    }
  }
};

}  // namespace audio_tools