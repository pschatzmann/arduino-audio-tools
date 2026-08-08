#pragma once
#include <math.h>

#include "AudioToolsConfig.h"
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/BaseConverter.h"
#include "AudioTools/CoreAudio/AudioBasic/soft_float_t.h"
#include "AudioTools/CoreAudio/AudioBasic/q1_14_t.h"

/**
 * @defgroup equilizer Equalizer
 * @ingroup dsp
 * @brief Digital Equalizer
 **/

namespace audio_tools {

/**
 * @brief Configuration for 3 Band Equalizer
 *
 * Configure the basic audio parameters (channels, bits_per_sample, sample_rate)
 * and the equalizer settings. The frequency and gain parameters apply to all
 * channels identically in the basic Equalizer3Bands class.
 *
 * Frequency bands:
 * - Low band: DC to freq_low Hz
 * - Medium band: freq_low to freq_high Hz
 * - High band: freq_high to Nyquist frequency
 *
 * Gain values should typically range from 0.0 to 2.0:
 * - 0.0 = complete attenuation (silence)
 * - 1.0 = no change (unity gain)
 * - 2.0 = 6dB boost
 *
 * @ingroup equilizer
 * @author pschatzmann
 */
struct ConfigEqualizer3Bands : public AudioInfo {
  ConfigEqualizer3Bands() {
    channels = 2;
    bits_per_sample = 16;
    sample_rate = 44100;
  }

  /// Low-pass filter cutoff frequency in Hz. Frequencies below this are
  /// considered "low"
  int freq_low = 880;

  /// High-pass filter cutoff frequency in Hz. Frequencies above this are
  /// considered "high"
  int freq_high = 5000;

  /// Gain multiplier for low frequencies (0.0-2.0, where 1.0 = unity gain)
  float gain_low = 1.0;

  /// Gain multiplier for medium frequencies (0.0-2.0, where 1.0 = unity gain)
  float gain_medium = 1.0;

  /// Gain multiplier for high frequencies (0.0-2.0, where 1.0 = unity gain)
  float gain_high = 1.0;
};

/// @cond DO_NOT_DOCUMENT

/**
 * @brief Assigns a coefficient computed in float (e.g. 2*sin(...), which is
 * always in the range [-2, 2)) into the equalizer's arithmetic type T,
 * logging via LOGE if it doesn't survive the round trip -- e.g. a bounded
 * fixed-point type like q1_14_t (range ~[-2,2)) silently clamping a value at
 * the edge of that range instead of representing it.
 * @ingroup equilizer
 */
template <typename T>
T eqAssignCoeff(float value, const char* name) {
  T t = value;
  float roundtrip = (float)t;
  if (fabs(roundtrip - value) > fabs(value) * 0.01f + 1e-4f) {
    LOGE(
        "Equalizer coefficient %s=%f does not fit in the range of T (stored "
        "as %f) - the equalizer will be inaccurate",
        name, value, roundtrip);
  }
  return t;
}

/**
 * @brief Denormal-avoidance offset added on every low/high-pass update
 * (`+ vsa`) to stop a native `float`/`double` FPU from stalling on denormal
 * numbers as the filter state decays toward zero. Bounded fixed-point/
 * software-float types (q1_14_t, soft_float_t) don't have this hardware
 * slowdown, so the offset is a no-op zero for them.
 * @ingroup equilizer
 */
template <typename T>
struct EqDenormalGuard {
  static T value() { return T(0); }
};
template <>
struct EqDenormalGuard<float> {
  static float value() { return 1.0f / 4294967295.0f; }
};
template <>
struct EqDenormalGuard<double> {
  static double value() { return 1.0 / 4294967295.0; }
};

/**
 * @brief Converts a raw PCM sample (type IntT) to/from the equalizer's
 * arithmetic type T. Defaults to FilterSampleConverter (see BaseConverter.h),
 * which is correct for T=soft_float_t (raw-magnitude convention, same as
 * float) and T=q1_14_t (bit-shifted into/out of its bounded ~[-2,2) range).
 * Specialized for T=float to keep the equalizer's original
 * NumberConverter-based, normalized-to-unity-range convention unchanged.
 * @ingroup equilizer
 */
template <typename IntT, typename T>
struct EqSampleConverter {
  static T toEqType(IntT sample, int bits) {
    return FilterSampleConverter<IntT, T>::toFilterType(sample);
  }
  static IntT fromEqType(T value, int bits) {
    return FilterSampleConverter<IntT, T>::fromFilterType(value);
  }
};
template <typename IntT>
struct EqSampleConverter<IntT, float> {
  static float toEqType(IntT sample, int bits) {
    return NumberConverter::toFloat((int32_t)sample, bits);
  }
  static IntT fromEqType(float value, int bits) {
    return (IntT)NumberConverter::fromFloat(value, bits);
  }
};

/// @endcond

/**
 * @brief 3 Band Equalizer with identical settings for all channels
 *
 * Digital 3-band equalizer implementation inspired from
 * https://www.musicdsp.org/en/latest/Filters/236-3-band-equaliser.html
 *
 * This equalizer applies the same frequency response and gain settings
 * to all audio channels. The audio spectrum is divided into three bands:
 * - Low: DC to freq_low Hz (controlled by gain_low)
 * - Medium: freq_low to freq_high Hz (controlled by gain_medium)
 * - High: freq_high to Nyquist frequency (controlled by gain_high)
 *
 * Each band uses a 4-pole filter implementation for smooth frequency response.
 * If you need different settings per channel, use Equalizer3BandsPerChannel
 * instead.
 *
 * After calling begin() you can update the gain parameters of the
 * ConfigEqualizer3Bands directly w/o calling any other methods. If you change
 * the freq. parameters, you need to call begin() again.
 * @warning begin(ConfigEqualizer3Bands&) needs a reference, so do not provide
 * it any stack objects!
 *
 * @tparam T Arithmetic type used for the filter state and coefficients.
 * Defaults to `float`. On a microcontroller without an FPU (e.g. RP2040),
 * use `q1_14_t` or `soft_float_t` instead (both in AudioBasic/) to avoid
 * software-emulated float instructions -- see the "Fixed-Point and
 * Software-Float Filter Types" section on the Filters wiki page. `Equalizer3Bands`
 * (no template argument) is a `float`-based alias kept for backwards
 * compatibility; instantiate `Equalizer3BandsT<q1_14_t>` or
 * `Equalizer3BandsT<soft_float_t>` directly for the fixed-point variants.
 * @ingroup equilizer
 * @author pschatzmann
 */
template <typename T = float>
class Equalizer3BandsT : public ModifyingStream {
 public:
  /// Constructor with Print output stream
  /// @param out Print stream where processed audio will be written
  Equalizer3BandsT(Print& out) { setOutput(out); }

  /// Constructor with bidirectional Stream
  /// @param in Stream for both input and output
  Equalizer3BandsT(Stream& in) { setStream(in); }

  /// Constructor with AudioOutput (includes automatic audio format
  /// notifications)
  /// @param out AudioOutput where processed audio will be written
  Equalizer3BandsT(AudioOutput& out) {
    setOutput(out);
    addNotifyAudioChange(out);
  }

  /// Constructor with AudioStream (includes automatic audio format
  /// notifications)
  /// @param stream AudioStream for both input and output
  Equalizer3BandsT(AudioStream& stream) {
    setStream(stream);
    addNotifyAudioChange(stream);
  }

  ~Equalizer3BandsT() {
    if (state != nullptr) delete[] state;
  }

  /// Defines/Changes the input & output stream
  /// @param io Stream to use for both reading and writing audio data
  void setStream(Stream& io) override {
    p_print = &io;
    p_stream = &io;
  };

  /// Defines/Changes the output target
  /// @param out Print stream where processed audio will be written
  void setOutput(Print& out) override { p_print = &out; }

  /// Access to the current configuration
  /// @return Reference to the configuration object
  ConfigEqualizer3Bands& config() { return *p_cfg; }

  /// Access to the default configuration
  /// @return Copy of the default configuration
  ConfigEqualizer3Bands defaultConfig() { return cfg; }

  /// Initialize the equalizer with the provided configuration
  /// @param config Configuration settings including frequencies and gains
  /// @return true if initialization was successful
  bool begin(ConfigEqualizer3Bands& config) {
    p_cfg = &config;
    return begin();
  }

  bool begin() {
    ModifyingStream::setAudioInfo(*p_cfg);

    if (p_cfg->channels > max_state_count) {
      if (state != nullptr) delete[] state;
      state = new EQSTATE[p_cfg->channels];
      max_state_count = p_cfg->channels;
    }

    // Setup state
    for (int j = 0; j < max_state_count; j++) {
      state[j] = EQSTATE();

      // Calculate filter cutoff frequencies
      state[j].lf = eqAssignCoeff<T>(
          2 * sin((float)PI * ((float)p_cfg->freq_low /
                                (float)p_cfg->sample_rate)),
          "lf");
      state[j].hf = eqAssignCoeff<T>(
          2 * sin((float)PI * ((float)p_cfg->freq_high /
                                (float)p_cfg->sample_rate)),
          "hf");
    }
    is_active = true;
    return true;
  }

  void end() { is_active = false; }

  /// Called automatically when audio format changes
  /// @param info New audio format information
  virtual void setAudioInfo(AudioInfo info) override {
    p_cfg->sample_rate = info.sample_rate;
    p_cfg->channels = info.channels;
    p_cfg->bits_per_sample = info.bits_per_sample;
    begin(*p_cfg);
  }

  /// Process and write audio data through the equalizer
  /// @param data Pointer to audio data buffer
  /// @param len Length of data in bytes
  /// @return Number of bytes written
  size_t write(const uint8_t* data, size_t len) override {
    filterSamples(data, len);
    return p_print->write(data, len);
  }

  /// Get available space for writing
  /// @return Number of bytes available for writing
  int availableForWrite() override { return p_print->availableForWrite(); }

  /// Read and process audio data through the equalizer
  /// @param data Buffer to store processed audio data
  /// @param len Maximum number of bytes to read
  /// @return Number of bytes actually read and processed
  size_t readBytes(uint8_t* data, size_t len) override {
    size_t result = 0;
    if (p_stream != nullptr) {
      result = p_stream->readBytes(data, len);
      filterSamples(data, len);
    }
    return result;
  }

  /// Get available data for reading
  /// @return Number of bytes available for reading
  int available() override {
    return p_stream != nullptr ? p_stream->available() : 0;
  }

 protected:
  bool is_active = false;     ///< Indicates if the equalizer is active
  ConfigEqualizer3Bands cfg;  ///< Default configuration instance
  ConfigEqualizer3Bands* p_cfg = &cfg;  ///< Pointer to active configuration
  const T vsa = EqDenormalGuard<T>::value();  ///< Denormal-avoidance offset
  Print* p_print = nullptr;    ///< Output stream for write operations
  Stream* p_stream = nullptr;  ///< Input/output stream for read operations
  int max_state_count = 0;     ///< Maximum number of allocated channel states

  /// Filter state for each channel
  struct EQSTATE {
    // Filter #1 (Low band) - 4-pole low-pass filter
    T lf;    ///< Low frequency cutoff coefficient
    T f1p0;  ///< Filter pole 0
    T f1p1;  ///< Filter pole 1
    T f1p2;  ///< Filter pole 2
    T f1p3;  ///< Filter pole 3

    // Filter #2 (High band) - 4-pole high-pass filter
    T hf;    ///< High frequency cutoff coefficient
    T f2p0;  ///< Filter pole 0
    T f2p1;  ///< Filter pole 1
    T f2p2;  ///< Filter pole 2
    T f2p3;  ///< Filter pole 3

    // Sample history buffer for filter calculations
    T sdm1;  ///< Sample data minus 1 (previous sample)
    T sdm2;  ///< Sample data minus 2
    T sdm3;  ///< Sample data minus 3

  }* state = nullptr;

  /// Apply 3-band equalization to audio samples
  /// @param data Pointer to audio data buffer (modified in-place)
  /// @param len Length of data buffer in bytes
  void filterSamples(const uint8_t* data, size_t len) {
    // no filter if not active
    if (!is_active) return;

    // process samples
    switch (p_cfg->bits_per_sample) {
      case 16: {
        int16_t* p_dataT = (int16_t*)data;
        size_t sample_count = len / sizeof(int16_t);
        for (size_t j = 0; j < sample_count; j += p_cfg->channels) {
          for (int ch = 0; ch < p_cfg->channels; ch++) {
            p_dataT[j + ch] = EqSampleConverter<int16_t, T>::fromEqType(
                sample(state[ch], EqSampleConverter<int16_t, T>::toEqType(
                                       p_dataT[j + ch], 16)),
                16);
          }
        }
      } break;
      case 24: {
        int24_t* p_dataT = (int24_t*)data;
        size_t sample_count = len / sizeof(int24_t);
        for (size_t j = 0; j < sample_count; j += p_cfg->channels) {
          for (int ch = 0; ch < p_cfg->channels; ch++) {
            p_dataT[j + ch] = EqSampleConverter<int24_t, T>::fromEqType(
                sample(state[ch], EqSampleConverter<int24_t, T>::toEqType(
                                       p_dataT[j + ch], 24)),
                24);
          }
        }
      } break;
      case 32: {
        int32_t* p_dataT = (int32_t*)data;
        size_t sample_count = len / sizeof(int32_t);
        for (size_t j = 0; j < sample_count; j += p_cfg->channels) {
          for (int ch = 0; ch < p_cfg->channels; ch++) {
            p_dataT[j + ch] = EqSampleConverter<int32_t, T>::fromEqType(
                sample(state[ch], EqSampleConverter<int32_t, T>::toEqType(
                                       p_dataT[j + ch], 32)),
                32);
          }
        }
      } break;

      default:
        LOGE("Only 16 bits supported: %d", p_cfg->bits_per_sample);
        break;
    }
  }

  /// Process a single audio sample through the 3-band equalizer
  /// @param es Reference to the filter state for this channel
  /// @param sample Input sample value
  /// @return Processed sample value with equalization applied
  T sample(EQSTATE& es, T sample) {
    // Locals
    T l, m, h;  // Low / Mid / High - Sample Values
    // Filter #1 (lowpass)
    es.f1p0 += (es.lf * (sample - es.f1p0)) + vsa;
    es.f1p1 += (es.lf * (es.f1p0 - es.f1p1));
    es.f1p2 += (es.lf * (es.f1p1 - es.f1p2));
    es.f1p3 += (es.lf * (es.f1p2 - es.f1p3));

    l = es.f1p3;

    // Filter #2 (highpass)
    es.f2p0 += (es.hf * (sample - es.f2p0)) + vsa;
    es.f2p1 += (es.hf * (es.f2p0 - es.f2p1));
    es.f2p2 += (es.hf * (es.f2p1 - es.f2p2));
    es.f2p3 += (es.hf * (es.f2p2 - es.f2p3));

    h = es.sdm3 - es.f2p3;
    // Calculate midrange (signal - (low + high))
    m = es.sdm3 - (h + l);
    // Scale, Combine and store
    l = l * p_cfg->gain_low;
    m = m * p_cfg->gain_medium;
    h = h * p_cfg->gain_high;

    // Shuffle history buffer
    es.sdm3 = es.sdm2;
    es.sdm2 = es.sdm1;
    es.sdm1 = sample;

    // Return result
    return (l + m + h);
  }
};

/// `float`-based alias kept for backwards compatibility; see Equalizer3BandsT
/// for the templated version used on FPU-less microcontrollers.
/// @ingroup equilizer
using Equalizer3Bands = Equalizer3BandsT<float>;

/**
 * @brief 3 Band Equalizer with per-channel frequency and gain control
 * Allows independent frequency and gain settings for each audio channel.
 * Each channel can have different low/high frequency cutoffs and different
 * gain values for low, medium, and high frequency bands.
 *
 * @tparam T Arithmetic type used for the filter state and coefficients; see
 * Equalizer3BandsT for details. `Equalizer3BandsPerChannel` (no template
 * argument) is a `float`-based alias kept for backwards compatibility.
 * @ingroup equilizer
 * @author pschatzmann
 */
template <typename T = float>
class Equalizer3BandsPerChannelT : public ModifyingStream {
 public:
  /// Constructor with Print output
  /// @param out Print stream for output
  Equalizer3BandsPerChannelT(Print& out) { setOutput(out); }

  /// Constructor with Stream input
  /// @param in Stream for input
  Equalizer3BandsPerChannelT(Stream& in) { setStream(in); }

  /// Constructor with AudioOutput
  /// @param out AudioOutput for output with automatic audio change
  /// notifications
  Equalizer3BandsPerChannelT(AudioOutput& out) {
    setOutput(out);
    out.addNotifyAudioChange(*this);
  }

  /// Constructor with AudioStream
  /// @param stream AudioStream for input/output with automatic audio change
  /// notifications
  Equalizer3BandsPerChannelT(AudioStream& stream) {
    setStream(stream);
    stream.addNotifyAudioChange(*this);
  }

  ~Equalizer3BandsPerChannelT() {
    if (state != nullptr) delete[] state;
  }

  /// Defines/Changes the input & output
  void setStream(Stream& io) override {
    p_print = &io;
    p_stream = &io;
  };

  /// Defines/Changes the output target
  void setOutput(Print& out) override { p_print = &out; }

  ConfigEqualizer3Bands& config() { return *p_cfg; }

  ConfigEqualizer3Bands defaultConfig() { return cfg; }

  /// Initialize the equalizer with the given configuration
  /// @param config Configuration settings for the equalizer
  /// @return true if initialization was successful
  bool begin(ConfigEqualizer3Bands& config) {
    p_cfg = &config;
    return begin();
  }

  /// Initialize the equalizer using the current configuration
  /// @return true if initialization was successful
  bool begin() {
    ModifyingStream::setAudioInfo(*p_cfg);
    // Ensure per-channel arrays are allocated
    ensureChannelArraysAllocated();

    // Ensure that EQSTATE is allocated
    if (p_cfg->channels > max_state_count) {
      if (state != nullptr) delete[] state;
      state = new EQSTATE[p_cfg->channels];
      max_state_count = p_cfg->channels;
    }

    // Setup state for each channel with its own parameters
    for (int j = 0; j < p_cfg->channels; j++) {
      state[j] = EQSTATE();

      // Calculate filter cutoff frequencies per channel
      state[j].lf = eqAssignCoeff<T>(
          2 * sin((float)PI *
                  ((float)freq_low[j] / (float)p_cfg->sample_rate)),
          "lf");
      state[j].hf = eqAssignCoeff<T>(
          2 * sin((float)PI *
                  ((float)freq_high[j] / (float)p_cfg->sample_rate)),
          "hf");
    }
    is_active = true;
    return true;
  }

  void end() override { is_active = false; }

  virtual void setAudioInfo(AudioInfo info) override {
    p_cfg->sample_rate = info.sample_rate;
    p_cfg->channels = info.channels;
    p_cfg->bits_per_sample = info.bits_per_sample;
    begin(*p_cfg);
  }

  /// Set frequency parameters for a specific channel
  /// @param channel Channel index (0-based)
  /// @param freq_low_val Low frequency cutoff in Hz
  /// @param freq_high_val High frequency cutoff in Hz
  void setChannelFrequencies(int channel, int freq_low_val, int freq_high_val) {
    ensureChannelArraysAllocated();
    if (channel >= 0 && channel < p_cfg->channels && !freq_low.empty()) {
      freq_low[channel] = freq_low_val;
      freq_high[channel] = freq_high_val;

      // Recalculate filter coefficients for this channel
      if (state != nullptr) {
        state[channel].lf = eqAssignCoeff<T>(
            2 * sin((float)PI *
                    ((float)freq_low_val / (float)p_cfg->sample_rate)),
            "lf");
        state[channel].hf = eqAssignCoeff<T>(
            2 * sin((float)PI *
                    ((float)freq_high_val / (float)p_cfg->sample_rate)),
            "hf");
      }
    }
  }

  /// Set gain parameters for a specific channel
  /// @param channel Channel index (0-based)
  /// @param gain_low_val Gain multiplier for low frequencies (0.0-2.0)
  /// @param gain_medium_val Gain multiplier for medium frequencies (0.0-2.0)
  /// @param gain_high_val Gain multiplier for high frequencies (0.0-2.0)
  void setChannelGains(int channel, float gain_low_val, float gain_medium_val,
                       float gain_high_val) {
    ensureChannelArraysAllocated();
    if (channel >= 0 && channel < p_cfg->channels && !gain_low.empty()) {
      gain_low[channel] = gain_low_val;
      gain_medium[channel] = gain_medium_val;
      gain_high[channel] = gain_high_val;
    }
  }

  /// Get frequency parameters for a specific channel
  /// @param channel Channel index (0-based)
  /// @param freq_low_val Reference to store the low frequency cutoff
  /// @param freq_high_val Reference to store the high frequency cutoff
  /// @return true if successful, false if channel index is invalid
  bool getChannelFrequencies(int channel, int& freq_low_val,
                             int& freq_high_val) {
    if (channel >= 0 && channel < p_cfg->channels && !freq_low.empty()) {
      freq_low_val = freq_low[channel];
      freq_high_val = freq_high[channel];
      return true;
    }
    return false;
  }

  /// Get gain parameters for a specific channel
  /// @param channel Channel index (0-based)
  /// @param gain_low_val Reference to store the low frequency gain
  /// @param gain_medium_val Reference to store the medium frequency gain
  /// @param gain_high_val Reference to store the high frequency gain
  /// @return true if successful, false if channel index is invalid
  bool getChannelGains(int channel, float& gain_low_val, float& gain_medium_val,
                       float& gain_high_val) {
    if (channel >= 0 && channel < p_cfg->channels && !gain_low.empty()) {
      gain_low_val = gain_low[channel];
      gain_medium_val = gain_medium[channel];
      gain_high_val = gain_high[channel];
      return true;
    }
    return false;
  }

  /// Process and write audio data through the per-channel equalizer
  /// @param data Pointer to audio data buffer
  /// @param len Length of data in bytes
  /// @return Number of bytes written
  size_t write(const uint8_t* data, size_t len) override {
    filterSamples(data, len);
    return p_print->write(data, len);
  }

  /// Get available space for writing
  /// @return Number of bytes available for writing
  int availableForWrite() override { return p_print->availableForWrite(); }

  /// Read and process audio data through the per-channel equalizer
  /// @param data Buffer to store processed audio data
  /// @param len Maximum number of bytes to read
  /// @return Number of bytes actually read and processed
  size_t readBytes(uint8_t* data, size_t len) override {
    size_t result = 0;
    if (p_stream != nullptr) {
      result = p_stream->readBytes(data, len);
      filterSamples(data, len);
    }
    return result;
  }

  /// Get available data for reading
  /// @return Number of bytes available for reading
  int available() override {
    return p_stream != nullptr ? p_stream->available() : 0;
  }

 protected:
  bool is_active = false;
  ConfigEqualizer3Bands cfg;            ///< Default configuration instance
  ConfigEqualizer3Bands* p_cfg = &cfg;  ///< Pointer to active configuration
  const T vsa = EqDenormalGuard<T>::value();  ///< Denormal-avoidance offset
  Print* p_print = nullptr;    ///< Output stream for write operations
  Stream* p_stream = nullptr;  ///< Input/output stream for read operations
  int max_state_count = 0;     ///< Maximum number of allocated channel states

  // Per-channel frequency and gain settings using Vector containers
  Vector<int> freq_low;       ///< Low frequency cutoffs per channel (Hz)
  Vector<int> freq_high;      ///< High frequency cutoffs per channel (Hz)
  Vector<float> gain_low;     ///< Low frequency gains per channel
  Vector<float> gain_medium;  ///< Medium frequency gains per channel
  Vector<float> gain_high;    ///< High frequency gains per channel

  struct EQSTATE {
    // Filter #1 (Low band)
    T lf;    // Frequency
    T f1p0;  // Poles ...
    T f1p1;
    T f1p2;
    T f1p3;

    // Filter #2 (High band)
    T hf;    // Frequency
    T f2p0;  // Poles ...
    T f2p1;
    T f2p2;
    T f2p3;

    // Sample history buffer
    T sdm1;  // Sample data minus 1
    T sdm2;  //                   2
    T sdm3;  //                   3

  }* state = nullptr;

  /// Ensures that per-channel arrays are allocated and properly sized
  void ensureChannelArraysAllocated() {
    if (freq_low.size() != p_cfg->channels) {
      allocateChannelArrays(p_cfg->channels);
    }
  }

  /// Allocates and initializes per-channel frequency and gain arrays
  /// @param num_channels Number of channels to allocate arrays for
  void allocateChannelArrays(int num_channels) {
    // Resize all vectors to accommodate the number of channels
    freq_low.resize(num_channels);
    freq_high.resize(num_channels);
    gain_low.resize(num_channels);
    gain_medium.resize(num_channels);
    gain_high.resize(num_channels);

    // Initialize with config default values
    for (int i = 0; i < num_channels; i++) {
      freq_low[i] = p_cfg->freq_low;
      freq_high[i] = p_cfg->freq_high;
      gain_low[i] = p_cfg->gain_low;
      gain_medium[i] = p_cfg->gain_medium;
      gain_high[i] = p_cfg->gain_high;
    }
  }

  /// Applies per-channel 3-band equalization to audio samples
  /// @param data Pointer to audio data buffer
  /// @param len Length of the data buffer in bytes
  void filterSamples(const uint8_t* data, size_t len) {
    if (!is_active) return;
    switch (p_cfg->bits_per_sample) {
      case 16: {
        int16_t* p_dataT = (int16_t*)data;
        size_t sample_count = len / sizeof(int16_t);
        for (size_t j = 0; j < sample_count; j += p_cfg->channels) {
          for (int ch = 0; ch < p_cfg->channels; ch++) {
            p_dataT[j + ch] = EqSampleConverter<int16_t, T>::fromEqType(
                sample(ch, EqSampleConverter<int16_t, T>::toEqType(
                               p_dataT[j + ch], 16)),
                16);
          }
        }
      } break;
      case 24: {
        int24_t* p_dataT = (int24_t*)data;
        size_t sample_count = len / sizeof(int24_t);
        for (size_t j = 0; j < sample_count; j += p_cfg->channels) {
          for (int ch = 0; ch < p_cfg->channels; ch++) {
            p_dataT[j + ch] = EqSampleConverter<int24_t, T>::fromEqType(
                sample(ch, EqSampleConverter<int24_t, T>::toEqType(
                               p_dataT[j + ch], 24)),
                24);
          }
        }
      } break;
      case 32: {
        int32_t* p_dataT = (int32_t*)data;
        size_t sample_count = len / sizeof(int32_t);
        for (size_t j = 0; j < sample_count; j += p_cfg->channels) {
          for (int ch = 0; ch < p_cfg->channels; ch++) {
            p_dataT[j + ch] = EqSampleConverter<int32_t, T>::fromEqType(
                sample(ch, EqSampleConverter<int32_t, T>::toEqType(
                               p_dataT[j + ch], 32)),
                32);
          }
        }
      } break;

      default:
        LOGE("Only 16 bits supported: %d", p_cfg->bits_per_sample);
        break;
    }
  }

  /// Processes a single sample through the 3-band equalizer for a specific
  /// channel
  /// @param channel The channel number to process
  /// @param sample_val The input sample value
  /// @return The processed sample value with per-channel equalization applied
  T sample(int channel, T sample_val) {
    EQSTATE& es = state[channel];

    // Locals
    T l, m, h;  // Low / Mid / High - Sample Values

    // Filter #1 (lowpass)
    es.f1p0 += (es.lf * (sample_val - es.f1p0)) + vsa;
    es.f1p1 += (es.lf * (es.f1p0 - es.f1p1));
    es.f1p2 += (es.lf * (es.f1p1 - es.f1p2));
    es.f1p3 += (es.lf * (es.f1p2 - es.f1p3));

    l = es.f1p3;

    // Filter #2 (highpass)
    es.f2p0 += (es.hf * (sample_val - es.f2p0)) + vsa;
    es.f2p1 += (es.hf * (es.f2p0 - es.f2p1));
    es.f2p2 += (es.hf * (es.f2p1 - es.f2p2));
    es.f2p3 += (es.hf * (es.f2p2 - es.f2p3));

    h = es.sdm3 - es.f2p3;

    // Calculate midrange (signal - (low + high))
    m = es.sdm3 - (h + l);

    // Scale with per-channel gains
    l = l * gain_low[channel];
    m = m * gain_medium[channel];
    h = h * gain_high[channel];

    // Shuffle history buffer
    es.sdm3 = es.sdm2;
    es.sdm2 = es.sdm1;
    es.sdm1 = sample_val;

    // Return result
    return (l + m + h);
  }
};

/// `float`-based alias kept for backwards compatibility; see
/// Equalizer3BandsPerChannelT for the templated version used on FPU-less
/// microcontrollers.
/// @ingroup equilizer
using Equalizer3BandsPerChannel = Equalizer3BandsPerChannelT<float>;

}  // namespace audio_tools
