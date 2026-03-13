#pragma once
#include <math.h>
#include <string.h>

#include <limits>

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioToolsConfig.h"

namespace audio_tools {

/**
 * @brief N-Band Equalizer using FIR filters with logarithmically spaced bands
 *
 * This class implements a graphic equalizer with configurable number of bands
 * using FIR (Finite Impulse Response) filtering. Frequency bands are
 * logarithmically spaced between 20Hz and Nyquist frequency.
 *
 * Features:
 * - Template-based configuration for number of taps and bands
 * - Fixed-point Q15 arithmetic for efficient processing on embedded systems
 * - Blackman windowing for reduced spectral leakage
 * - Per-band gain control (-90dB to +12dB)
 * - Multi-channel support
 * - Double-buffered kernel for thread-safe real-time updates
 *
 * Thread Safety:
 * Uses double-buffering with two kernel buffers. Audio processing reads from
 * the active kernel while updates are written to the inactive kernel. An atomic
 * pointer swap ensures glitch-free transitions without blocking audio
 * processing.
 *
 * @tparam NUM_TAPS Number of FIR filter taps (higher = sharper transitions,
 *                  more CPU usage). Typical values: 64, 128, 256
 * @tparam NUM_BANDS Number of frequency bands. Typical values: 3, 5, 10, 31
 *
 *
 * @note NUM_TAPS should be odd for symmetric filter response
 * @note Higher NUM_TAPS values require more memory and processing time
 */
template <typename SampleT = int16_t, typename AccT = int64_t,
          int NUM_TAPS = 128, int NUM_BANDS = 12>
class EqualizerNBands : public ModifyingStream {
 public:
  EqualizerNBands() { setBandGains(0.0f); };
  /// Constructor with Print output
  /// @param out Print stream for output
  EqualizerNBands(Print& out) : EqualizerNBands() { setOutput(out); }

  /// Constructor with Stream input
  /// @param in Stream for input
  EqualizerNBands(Stream& in) : EqualizerNBands() { setStream(in); }

  /// Constructor with AudioOutput
  /// @param out AudioOutput for output with automatic audio change
  /// notifications
  EqualizerNBands(AudioOutput& out) : EqualizerNBands() {
    setOutput(out);
    out.addNotifyAudioChange(*this);
  }

  /// Constructor with AudioStream
  /// @param stream AudioStream for input/output with automatic audio change
  /// notifications
  EqualizerNBands(AudioStream& stream) : EqualizerNBands() {
    setStream(stream);
    stream.addNotifyAudioChange(*this);
  }

  ~EqualizerNBands() { end(); }

  /// Defines/Changes the input & output stream
  /// @param io Stream to use for both reading and writing audio data
  void setStream(Stream& io) override {
    p_print = &io;
    p_stream = &io;
  };

  /// Defines/Changes the output target
  /// @param out Print stream where processed audio will be written
  void setOutput(Print& out) override { p_print = &out; }

  bool begin(AudioInfo info) {
    setAudioInfo(info);
    return begin();
  }

  /// Initializes the equalizer with the current audio info
  bool begin() override {
    currentSampleRate = audioInfo().sample_rate;
    if (currentSampleRate <= 0) {
      LOGE("Invalid sample rate: %d", currentSampleRate);
      return false;
    }
    setupFrequencies(currentSampleRate);
    preCalculateWindow();

    // Initialize double-buffering pointers
    activeKernel = kernelA;
    updateKernel = kernelB;

    // Initialize both kernels with pass-through (identity) filter
    initializeKernel(kernelA);
    initializeKernel(kernelB);

    // assign output or source
    if (p_stream) filtered.setStream(*p_stream);
    if (p_print) filtered.setOutput(*p_print);
    filtered.begin(audioInfo());

    // set filters for all channels
    fir_vector.resize(audioInfo().channels);
    for (int ch = 0; ch < audioInfo().channels; ch++) {
      fir_vector[ch].setKernel(activeKernel);
      filtered.setFilter(ch, &fir_vector[ch]);
    }

    // calculate the kernel
    bool rc = updateFIRKernel();

    // Log the initial band settings for visibility
    for (int band = 0; band < NUM_BANDS; band++) {
      LOGI("Band %d: Freq=%.2fHz, Gain=%.2fdB", band, getBandFrequency(band),
           getBandDB(band));
    }
    return rc;
  }

  void end() {
    fir_vector.clear();
    currentSampleRate = 0;
  }

  /// Set gain for a specific frequency band
  /// @param band Band index (0 to NUM_BANDS-1)
  /// @param volume Volume level (-1.0 to 1.0) mapped to -12dB to +12dB.
  ///        For deeper cuts use setBandDB() directly.
  bool setBandGain(int band, float volume) {
    // Map -1.0 to 1.0 to -90DB to +12dB
    float vol_db = volume < 0 ? map<float>(volume, -1.0f, 0.0f, -90.0f, 0.0f) : map<float>(volume, 0.0f, 1.0f, 0.0f, 12.0f);
    return setBandDB(band, vol_db);
  }

  /// Set gain for a specific band directly in dB
  /// @param band Band index (0 to NUM_BANDS-1)
  /// @param gainDb Gain in dB (-90 to +12). The lower limit matches Q15
  ///        dynamic range; the upper limit prevents clipping.
  bool setBandDB(int band, float gainDb) {
    if (band < 0 || band >= NUM_BANDS) return false;
    float db = min(gainDb, 12.0f);
    db = max(db, -90.0f);
    pendingGains[band] = db;
    gainsDirty = true;
    return true;
  }

  /// Set same gain for all frequency bands
  bool setBandGains(float volume) {
    for (int band = 0; band < NUM_BANDS; band++) {
      setBandGain(band, volume);
    }
    return true;
  }

  /// Get current gain for a specific band as normalized volume (-1.0 to 1.0)
  float getBandGain(int band) {
    if (band < 0 || band >= NUM_BANDS) return 0.0f;
    return map<float>(pendingGains[band], -12.0f, 12.0f, -1.0f, 1.0f);
  }

  /// Get current gain for a specific band in dB
  /// @param band Band index (0 to NUM_BANDS-1)
  /// @return Gain in dB (-90 to +12), or 0 if band index invalid
  float getBandDB(int band) const {
    if (band < 0 || band >= NUM_BANDS) return 0.0f;
    return pendingGains[band];
  }

  /// Get center frequency for a specific band
  /// @param band Band index (0 to NUM_BANDS-1)
  /// @return Center frequency in Hz, or 0 if band index invalid
  float getBandFrequency(int band) const {
    if (band < 0 || band >= NUM_BANDS) return 0.0f;
    return centerFreqs[band];
  }

  /// Get number of bands
  int getBandCount() const { return NUM_BANDS; }

  /// Enable/disable automatic kernel updates during read/write
  /// @param enabled When true, pending gain changes are applied automatically
  void setAutoUpdate(bool enabled) { autoUpdate = enabled; }

  // update the FIR kernel after changing gains
  bool update() { return updateFIRKernel(); }

  size_t write(const uint8_t* data, size_t len) override {
    maybeUpdateKernel();
    return filtered.write(data, len);
  }

  size_t readBytes(uint8_t* data, size_t len) override {
    maybeUpdateKernel();
    return filtered.readBytes(data, len);
  }

 protected:
  // Simple re-entrancy guard: prevents concurrent kernel updates from
  // corrupting scratch buffers / updateKernel.
  volatile bool isUpdating = false;
  // Indicates new gains are pending and kernel should be refreshed.
  volatile bool gainsDirty = false;
  // Auto-update kernel during streaming. Default is false to preserve
  // explicit update() behavior.
  bool autoUpdate = false;

  // Custom FIR Filter Class
  class EQFIRFilter : public Filter<SampleT> {
   public:
    EQFIRFilter() : activeKernel(nullptr) {}

    void setKernel(volatile int16_t* kernel) { activeKernel = kernel; }

    SampleT process(SampleT sample) override {
      if (activeKernel == nullptr) {
        LOGE("Kernel not set!");
        return sample;  // Pass-through if no kernel set
      }

      xHistory[idxHist] = sample;

      // Use AccT to prevent overflow and/or allow float accumulation
      AccT acc = (AccT)0;
      int idx = idxHist;

      for (int n = 0; n < NUM_TAPS; n++) {
        // Coefficients are Q15 int16_t
        acc += (AccT)xHistory[idx] * (AccT)activeKernel[n];
        if (--idx < 0) idx = NUM_TAPS - 1;
      }

      if (++idxHist >= NUM_TAPS) idxHist = 0;

      // Convert back from Q15 and saturate
      return fromQ15(acc);
    }

   protected:
    SampleT xHistory[NUM_TAPS] = {(SampleT)0};
    int idxHist = 0;
    volatile int16_t* activeKernel = nullptr;  // Pointer to active kernel

    static inline SampleT fromQ15(AccT acc) {
      // Default path: integer SampleT (current behavior)
      if constexpr (std::numeric_limits<SampleT>::is_integer) {
        // Shift back from Q15
        if constexpr (std::numeric_limits<AccT>::is_integer) {
          acc >>= 15;
        } else {
          acc = acc / (AccT)(1 << 15);
        }

        // Saturation to SampleT range
        const AccT hi = (AccT)std::numeric_limits<SampleT>::max();
        const AccT lo = (AccT)std::numeric_limits<SampleT>::min();
        if (acc > hi) acc = hi;
        if (acc < lo) acc = lo;
        return (SampleT)acc;
      } else {
        // Floating output sample: scale Q15 back to approximately [-1..1]
        return (SampleT)(acc / (AccT)(1 << 15));
      }
    }
  };

  // Q15 range is [-32768, 32767]. Use 32767 for +1.0 to avoid overflow/wrap.
  static constexpr float Q15_SCALE = 32767.0f;
  float centerFreqs[NUM_BANDS];
  // Gains in dB used by the kernel design.
  float gains[NUM_BANDS] = {0};
  // Gains written by user-facing setters. Copied to gains transactionally.
  float pendingGains[NUM_BANDS] = {0};

  // Scratch buffer used during kernel design. Kept as member to avoid static
  // storage (shared across instances and non-reentrant).
  float tempFloat[NUM_TAPS] = {0};

  // Double-buffering: two kernels for thread-safe updates
  int16_t kernelA[NUM_TAPS];
  int16_t kernelB[NUM_TAPS];
  volatile int16_t* activeKernel;  // Pointer to currently used kernel
  volatile int16_t* updateKernel;  // Pointer to kernel being updated

  float windowCoeffs[NUM_TAPS];  // Pre-calculated Blackman window
  int currentSampleRate = 0;

  Print* p_print = nullptr;        ///< Output stream for write operations
  Stream* p_stream = nullptr;      ///< Input stream for read operations
  Vector<EQFIRFilter> fir_vector;  ///< Vector of FIR filters for each channel
  FilteredStream<SampleT, SampleT> filtered;

  // Centralized interrupt guards to avoid repeated #if defined(ARDUINO) blocks.
  inline void enterCritical() {
#if defined(ARDUINO)
    noInterrupts();
#endif
  }

  inline void exitCritical() {
#if defined(ARDUINO)
    interrupts();
#endif
  }

  // Update kernel if any gain changes are pending.
  inline void maybeUpdateKernel() {
    if (!autoUpdate) return;
    if (gainsDirty) {
      if (updateFIRKernel()) {
        gainsDirty = false;
      }
    }
  }

  template <typename T>
  float map(T x, T in_min, T in_max, T out_min, T out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  }

  // Helper Sinc
  float sinc(float x) {
    if (fabsf(x) < 1e-8f) return 1.0f;
    return sinf(PI * x) / (PI * x);
  }

  // Setup Center Frequencies Logarithmically spaced between 20Hz and Nyquist
  void setupFrequencies(int sampleRate) {
    if (NUM_BANDS <= 0) return;
    float fMin = log10f(20.0f);
    float fMax = log10f(sampleRate / 2.0f);  // Nyquist frequency
    if (NUM_BANDS == 1) {
      centerFreqs[0] = powf(10.0f, (fMin + fMax) * 0.5f);
      LOGD("Only one band: center frequency set to %.2f Hz", centerFreqs[0]);
      return;
    }
    float step = (fMax - fMin) / (float)(NUM_BANDS - 1);
    for (int i = 0; i < NUM_BANDS; i++) {
      centerFreqs[i] = powf(10.0f, fMin + step * (float)i);
      LOGD("Band %d: center frequency = %.2f Hz", i, centerFreqs[i]);
    }
  }

  // Pre-calculate Blackman window coefficients (performance optimization)
  void preCalculateWindow() {
    const float N_minus_1 = (float)(NUM_TAPS - 1);
    for (int n = 0; n < NUM_TAPS; n++) {
      windowCoeffs[n] = 0.42f - 0.5f * cosf(2.0f * PI * n / N_minus_1) +
                        0.08f * cosf(4.0f * PI * n / N_minus_1);
    }
  }

  // Initialize a kernel to pass-through (identity filter)
  void initializeKernel(volatile int16_t* kernel) {
    const int M = (NUM_TAPS - 1) / 2;
    for (int i = 0; i < NUM_TAPS; i++) {
      if (i == M) {
        kernel[i] = (int16_t)Q15_SCALE;  // Unity gain at center tap
      } else {
        kernel[i] = 0;
      }
    }
  }

  bool updateFIRKernel() {
    if (currentSampleRate <= 0) {
      LOGE("Invalid sample rate: %d", currentSampleRate);
      return false;  // Not initialized yet
    }

    // Prevent concurrent updates (e.g., from multiple tasks/threads).
    // We keep the critical section tiny: only the check/set of the flag.
    enterCritical();
    if (isUpdating) {
      exitCritical();
      return false;
    }
    isUpdating = true;
    exitCritical();

    // Transactional gain update: copy user-updated pendingGains into gains.
    // Keep this critical section short to avoid blocking audio processing.
    enterCritical();
    memcpy(gains, pendingGains, sizeof(gains));
    exitCritical();

    memset(tempFloat, 0, sizeof(tempFloat));

    const int M = (NUM_TAPS - 1) / 2;
    const float sampleRateFloat = (float)currentSampleRate;

    // Base impulse (Pass-through)
    tempFloat[M] = 1.0f;

    for (int i = 0; i < NUM_BANDS; i++) {
      // Skip bands with 0dB gain to save precision
      if (fabs(gains[i]) < 0.1f) continue;

      // Calculate Linear Gain Delta
      // If gain is +6dB (2.0x), we add (2.0 - 1.0) = +1.0 to the impulse
      float linGain = powf(10.0f, gains[i] / 20.0f) - 1.0f;

      // Use actual sample rate
      float fL_hz = centerFreqs[i] * 0.707f;  // Lower Edge (-3dB point)
      float fH_hz = centerFreqs[i] * 1.414f;  // Upper Edge (+3dB point)

      // Enforce minimum bandwidth so the windowed-sinc FIR can resolve
      // this band.  The Blackman window main-lobe width is ~4/N in
      // normalised frequency, i.e. 4*Fs/N Hz.  Bands narrower than that
      // are effectively invisible to the filter and produce a near-flat
      // (no-effect) response.
      float minBwHz = 4.0f * sampleRateFloat / (float)NUM_TAPS;
      float actualBwHz = fH_hz - fL_hz;
      if (actualBwHz < minBwHz) {
        float expand = (minBwHz - actualBwHz) * 0.5f;
        fL_hz = fL_hz - expand;
        fH_hz = fH_hz + expand;
        if (fL_hz < 1.0f) fL_hz = 1.0f;
      }

      float fL = fL_hz / sampleRateFloat;
      float fH = fH_hz / sampleRateFloat;

      // Clamp to valid normalized frequency range [0, 0.5] (Nyquist)
      if (fL < 0.0f) fL = 0.0f;
      if (fH < 0.0f) fH = 0.0f;
      if (fL > 0.5f) fL = 0.5f;
      if (fH > 0.5f) fH = 0.5f;
      if (fH <= fL) continue;

      // Evaluate the windowed bandpass magnitude at the center frequency
      // so we can normalise to unity.  The Blackman window reduces the
      // passband peak below 1.0; without compensation the actual
      // boost/cut is weaker than requested.
      float wCenter = 2.0f * PI * centerFreqs[i] / sampleRateFloat;
      float hReal = 0.0f;
      float hImag = 0.0f;
      for (int n = 0; n < NUM_TAPS; n++) {
        float nM = (float)(n - M);
        float bpW = ((2.0f * fH * sinc(2.0f * fH * nM)) -
                     (2.0f * fL * sinc(2.0f * fL * nM))) *
                    windowCoeffs[n];
        hReal += bpW * cosf(wCenter * n);
        hImag -= bpW * sinf(wCenter * n);
      }
      float bpMag = sqrtf(hReal * hReal + hImag * hImag);
      float normFactor = (bpMag > 1e-6f) ? (1.0f / bpMag) : 1.0f;

      for (int n = 0; n < NUM_TAPS; n++) {
        float nM = (float)(n - M);

        // Use pre-calculated window coefficients
        float window = windowCoeffs[n];

        // Bandpass filter: highpass - lowpass
        float bp = (2.0f * fH * sinc(2.0f * fH * nM)) -
                   (2.0f * fL * sinc(2.0f * fL * nM));

        // Add the normalised, weighted bandpass to the master kernel
        tempFloat[n] += bp * window * normFactor * linGain;
      }
    }

    // Update the inactive kernel (no interruption needed for writes)
    for (int i = 0; i < NUM_TAPS; i++) {
      // DIRECT CONVERSION (No Auto-Normalization)
      // This ensures +6dB actually outputs louder voltage
      int32_t q = (int32_t)(tempFloat[i] * Q15_SCALE);

      // Hard Clip the Kernel Coefficients to prevent wrap-around
      if (q > 32767) q = 32767;
      if (q < -32768) q = -32768;
      updateKernel[i] = (int16_t)q;
    }

    // Atomically swap the kernel pointers
    // This is the only operation that needs to be atomic
    enterCritical();
    volatile int16_t* temp = activeKernel;
    activeKernel = updateKernel;
    updateKernel = temp;

    // Update filter references to new active kernel
    for (auto& fir : fir_vector) {
      fir.setKernel(activeKernel);
    }
    exitCritical();

    // Release re-entrancy guard
    enterCritical();
    isUpdating = false;
    gainsDirty = false;
    exitCritical();

    LOGI("FIR kernel updated with new gains for %d bands /%d taps.", NUM_BANDS,
         NUM_TAPS);
    return true;
  }
};

}  // namespace audio_tools
