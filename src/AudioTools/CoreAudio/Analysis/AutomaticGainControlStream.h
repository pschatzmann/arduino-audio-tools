#pragma once

#include <math.h>

#include "AudioTools/CoreAudio/AudioBasic/q1_14_t.h"
#include "AudioTools/CoreAudio/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Configuration for AutomaticGainControlStream
 *
 * This structure extends AudioInfo to include parameters for automatic gain
 * control. It defines the target loudness, gain limits, attack/release times,
 * and other settings that control how the stream adjusts audio levels.
 *
 * @ingroup dsp
 * @author pschatzmann
 */

struct AutomaticGainControlStreamConfig : public AudioInfo {
  // Desired average RMS level.
  float target_db = -18.0f;

  // Maximum automatic amplification.
  float max_gain_db = 6.0f;

  // Maximum automatic attenuation.
  float min_gain_db = -24.0f;

  // Time to reduce gain when the input gets louder.
  float attack_seconds = 0.25f;

  // Time to increase gain when the input gets quieter.
  //
  // Keep this relatively long to preserve fades.
  float release_seconds = 3.0f;

  // Below this RMS level the signal is considered silence.
  //
  // The normalizer will NOT increase gain below this level.
  float silence_db = -55.0f;

  // Maximum allowed peak after normalization.
  //
  // -1 dBFS leaves a small amount of digital headroom.
  float peak_headroom_db = -1.0f;

  // Reset gain and detector state when AudioInfo changes.
  bool reset_on_audio_change = true;
};

/**
 * PCM type traits used by AutomaticGainControlStream.
 *
 * Declared at namespace scope (rather than as a nested member template)
 * because explicit specialization of a member template inside a class body
 * is not standard C++ and is rejected by some compilers.
 *
 * fixed_shift is the right-shift that normalizes a sample to ~16-bit range
 * before squaring in the PREFER_FIXEDPOINT accumulation path, so the sum of
 * squares cannot overflow int64_t even for large blocks.
 */
template <typename T>
struct AGCPCMTraits;

template <>
struct AGCPCMTraits<int16_t> {
  static constexpr float max_sample = 32768.0f;
  static constexpr float min_value = -32768.0f;
  static constexpr float max_value = 32767.0f;
  static constexpr int fixed_shift = 0;
};

template <>
struct AGCPCMTraits<int24_t> {
  static constexpr float max_sample = 8388608.0f;
  static constexpr float min_value = -8388608.0f;
  static constexpr float max_value = 8388607.0f;
  static constexpr int fixed_shift = 8;
};

template <>
struct AGCPCMTraits<int32_t> {
  static constexpr float max_sample = 2147483648.0f;
  static constexpr float min_value = -2147483648.0f;
  static constexpr float max_value = 2147483647.0f;
  static constexpr int fixed_shift = 16;
};

/**
 * @brief Slow automatic loudness normalization for PCM audio.
 *
 * The stream measures the RMS level of the incoming PCM signal and
 * adjusts an automatic gain towards a target level.
 *
 * Gain reduction is relatively fast (attack), while gain increase is
 * deliberately slow (release). This prevents the normalizer from
 * destroying natural fades.
 *
 * Recommended signal chain:
 *
 *   Decoder -> AutomaticGainControlStream -> VolumeStream -> I2S
 *
 * The normalizer operates on decoded PCM data.
 */
class AutomaticGainControlStream : public ModifyingStream {
 public:
  /// Default constructor: call setStream()/setOutput() before begin()
  AutomaticGainControlStream() = default;

  /// Constructs the stream with a bidirectional Stream as input and output
  explicit AutomaticGainControlStream(Stream& out) { setOutput(out); }

  /// Constructs the stream with an AudioStream as output and registers it
  /// for AudioInfo change notifications
  explicit AutomaticGainControlStream(AudioStream& out) {
    setStream(out);
  }

  /// Constructs the stream with an AudioOutput as output and registers it
  /// for AudioInfo change notifications
  explicit AutomaticGainControlStream(AudioOutput& out) {
    setOutput(out);
  }

  /// Defines the input Stream; output is set to the same Stream
  void setStream(Stream& in) override {
    p_in = &in;
    p_out = p_in;
  }

  /// Defines the output Print target
  void setOutput(Print& out) override { p_out = &out; }

  /// Provides the default configuration
  AutomaticGainControlStreamConfig defaultConfig() {
    return AutomaticGainControlStreamConfig();
  }

  /// Starts processing using the current AudioInfo
  bool begin() override { return begin(info); }

  /// Starts processing with the given AudioInfo, keeping the current config
  bool begin(AudioInfo cfg) {
    setAudioInfo(cfg);
    reset();
    is_started = true;
    return true;
  }

  /// Starts processing with the given AutomaticGainControlStreamConfig
  bool begin(AutomaticGainControlStreamConfig cfg) {
    config = cfg;
    setAudioInfo(info);
    reset();
    is_started = true;
    return true;
  }

  /// Stops processing; the underlying Stream/Print is left untouched
  void end() override { is_started = false; }

  /// Updates the configuration without resetting the current gain state
  void setConfig(const AutomaticGainControlStreamConfig& cfg) { config = cfg; }

  /// Provides access to the current configuration
  AutomaticGainControlStreamConfig& getConfig() { return config; }

  /// Resets the current gain and level detector state
  void reset() {
    current_gain_db = 0.0f;
    measured_db = -100.0f;
    has_level = false;
  }

  /// Updates the AudioInfo; resets state when reset_on_audio_change is set
  void setAudioInfo(AudioInfo cfg) override {
    ModifyingStream::setAudioInfo(cfg);

    if (config.reset_on_audio_change) {
      reset();
    }

    sample_rate = cfg.sample_rate;
    channels = cfg.channels;
    bits_per_sample = cfg.bits_per_sample;

    if (sample_rate == 0) {
      sample_rate = 44100;
    }
  }

  /// Reads and gain-adjusts PCM data from the input Stream
  size_t readBytes(uint8_t* data, size_t len) override {
    if (data == nullptr || p_in == nullptr) {
      return 0;
    }

    size_t result = p_in->readBytes(data, len);

    if (result > 0 && is_started) {
      process(data, result);
    }

    return result;
  }

  /// Gain-adjusts PCM data in place and writes it to the output
  size_t write(const uint8_t* data, size_t len) override {
    if (data == nullptr || p_out == nullptr) {
      return 0;
    }

    if (is_started && len > 0) {
      process(data, len);
    }

    return p_out->write(data, len);
  }

  /// Provides the available space of the output
  int availableForWrite() override {
    return p_out == nullptr ? 0 : p_out->availableForWrite();
  }

  /// Provides the available data of the input
  int available() override { return p_in == nullptr ? 0 : p_in->available(); }

 protected:
  Print* p_out = nullptr;
  Stream* p_in = nullptr;
  AutomaticGainControlStreamConfig config;
  bool is_started = false;
  uint32_t sample_rate = 44100;
  uint16_t channels = 2;
  uint16_t bits_per_sample = 16;
  float current_gain_db = 0.0f;
  float measured_db = -100.0f;
  bool has_level = false;

  static float dbToLinear(float db) { return powf(10.0f, db / 20.0f); }

  static float linearToDb(float value) {
    if (value <= 0.0000001f) {
      return -100.0f;
    }

    return 20.0f * log10f(value);
  }

  /**
   * Calculate the smoothing coefficient for the current block.
   *
   * The coefficient is based on the actual block duration, so the
   * attack/release behaviour is largely independent of StreamCopy's
   * buffer size.
   */
  float smoothingCoefficient(float seconds, size_t samples) const {
    if (seconds <= 0.0f) {
      return 1.0f;
    }

    if (sample_rate == 0 || channels == 0 || samples == 0) {
      return 1.0f;
    }

    float block_seconds = static_cast<float>(samples) /
                          static_cast<float>(sample_rate * channels);

    return 1.0f - expf(-block_seconds / seconds);
  }

  /**
   * Calculate RMS and peak for any supported integer PCM type.
   *
   * Under PREFER_FIXEDPOINT the per-sample loop accumulates plain integer
   * sums (samples are pre-shifted via AGCPCMTraits<T>::fixed_shift so the
   * sum of squares cannot overflow int64_t), and float math (sqrtf) is
   * only used once per block to derive rms/peak. This avoids per-sample
   * FPU work on microcontrollers that lack a hardware float unit.
   */
  template <typename T>
  void analyze(const T* data, size_t samples, float& rms, float& peak) {
#if PREFER_FIXEDPOINT
    constexpr int shift = AGCPCMTraits<T>::fixed_shift;
    constexpr float max_sample = AGCPCMTraits<T>::max_sample;

    int64_t sum_sq = 0;
    int32_t peak_raw = 0;

    for (size_t i = 0; i < samples; ++i) {
      // int24_t's conversion operators are not const-qualified, so a
      // local (non-const) copy is needed before the cast.
      T sample = data[i];
      int32_t value = static_cast<int32_t>(sample) >> shift;
      int32_t abs_value = value < 0 ? -value : value;

      if (abs_value > peak_raw) {
        peak_raw = abs_value;
      }

      sum_sq += static_cast<int64_t>(value) * value;
    }

    float scaled_max = max_sample / static_cast<float>(1 << shift);

    rms = samples == 0 ? 0.0f
                        : sqrtf(static_cast<float>(sum_sq) /
                                static_cast<float>(samples)) /
                              scaled_max;
    peak = static_cast<float>(peak_raw) / scaled_max;
#else
    float sum = 0.0f;
    peak = 0.0f;

    constexpr float max_sample = AGCPCMTraits<T>::max_sample;

    for (size_t i = 0; i < samples; ++i) {
      // int24_t's conversion operators are not const-qualified, so a
      // local (non-const) copy is needed before the cast.
      T sample = data[i];
      float value = static_cast<float>(sample) / max_sample;

      float abs_value = fabsf(value);

      sum += value * value;

      if (abs_value > peak) {
        peak = abs_value;
      }
    }

    rms = samples == 0 ? 0.0f : sqrtf(sum / static_cast<float>(samples));
#endif
  }

  /**
   * Dispatch PCM analysis according to bits_per_sample.
   */
  bool analyze(const uint8_t* buffer, size_t bytes, float& rms, float& peak) {
    if (channels == 0) {
      return false;
    }

    switch (bits_per_sample) {
      case 16:
        analyze(reinterpret_cast<const int16_t*>(buffer),
                bytes / sizeof(int16_t), rms, peak);
        return true;

      case 24:
        analyze(reinterpret_cast<const int24_t*>(buffer),
                bytes / sizeof(int24_t), rms, peak);
        return true;

      case 32:
        analyze(reinterpret_cast<const int32_t*>(buffer),
                bytes / sizeof(int32_t), rms, peak);
        return true;

      default:
        return false;
    }
  }

  /**
   * Calculate the desired automatic gain.
   */
  float desiredGainDb(float rms_db, float peak_db) const {
    /*
     * Do not amplify silence.
     *
     * This is particularly important for fades and prevents
     * background noise from being amplified after a song becomes
     * quiet.
     */
    if (rms_db <= config.silence_db) {
      return current_gain_db;
    }

    /*
     * RMS normalization.
     *
     * Example:
     *
     * target  = -18 dB
     * measured = -24 dB
     *
     * desired = +6 dB
     */
    float desired = config.target_db - rms_db;

    /*
     * Automatic gain limits.
     */
    if (desired > config.max_gain_db) {
      desired = config.max_gain_db;
    }

    if (desired < config.min_gain_db) {
      desired = config.min_gain_db;
    }

    /*
     * Peak protection.
     *
     * Never intentionally request enough gain to put the detected
     * peak above the configured headroom.
     */
    float peak_after_gain = peak_db + desired;

    if (peak_after_gain > config.peak_headroom_db) {
      float peak_limited_gain = config.peak_headroom_db - peak_db;

      if (peak_limited_gain < desired) {
        desired = peak_limited_gain;
      }
    }

    /*
     * Peak limiting can have pushed the gain below our configured
     * lower bound, so apply the minimum one final time.
     */
    if (desired < config.min_gain_db) {
      desired = config.min_gain_db;
    }

    return desired;
  }

  void updateGain(float desired_gain_db, size_t samples) {
    float coefficient;

    if (desired_gain_db < current_gain_db) {
      /*
       * Input got louder.
       *
       * Reduce gain relatively quickly.
       */
      coefficient = smoothingCoefficient(config.attack_seconds, samples);

    } else {
      /*
       * Input got quieter.
       *
       * Increase gain slowly.
       *
       * This is what preserves fades.
       */
      coefficient = smoothingCoefficient(config.release_seconds, samples);
    }

    current_gain_db += coefficient * (desired_gain_db - current_gain_db);

    /*
     * Avoid unnecessary floating point movement when we are
     * already effectively at the target.
     */
    if (fabsf(current_gain_db - desired_gain_db) < 0.001f) {
      current_gain_db = desired_gain_db;
    }
  }

  /**
   * Apply gain to any supported PCM type.
   *
   * By default the calculation is performed in float precision and the
   * result is saturated to the PCM range.
   *
   * Under PREFER_FIXEDPOINT the gain is converted to a Q1.14 fixed-point
   * factor once per block, and q1_14_t::scale() applies it per sample
   * using pure integer multiply/shift, avoiding an FPU multiply on every
   * sample. Q1.14 covers roughly [-2.0, 1.99994], which comfortably fits
   * the default max_gain_db (+6 dB, ~1.995x); a max_gain_db configured
   * well beyond that will saturate at the Q1.14 limit under
   * PREFER_FIXEDPOINT even though the float path would not.
   */
  template <typename T>
  void apply(T* data, size_t samples, float gain) {
#if PREFER_FIXEDPOINT
    q1_14_t fixed_gain(gain);

    for (size_t i = 0; i < samples; ++i) {
      data[i] = fixed_gain.scale(data[i]);
    }
#else
    constexpr float min_value = AGCPCMTraits<T>::min_value;

    constexpr float max_value = AGCPCMTraits<T>::max_value;

    const float dgain = static_cast<float>(gain);

    for (size_t i = 0; i < samples; ++i) {
      float value = static_cast<float>(data[i]) * dgain;

      if (value > max_value) {
        value = max_value;
      }

      if (value < min_value) {
        value = min_value;
      }

      data[i] = static_cast<T>(value);
    }
#endif
  }

  /**
   * Dispatch PCM gain application according to bits_per_sample.
   */
  void apply(uint8_t* buffer, size_t bytes, float gain) {
    switch (bits_per_sample) {
      case 16:
        apply(reinterpret_cast<int16_t*>(buffer), bytes / sizeof(int16_t),
              gain);
        break;

      case 24:
        apply(reinterpret_cast<int24_t*>(buffer), bytes / sizeof(int24_t),
              gain);
        break;

      case 32:
        apply(reinterpret_cast<int32_t*>(buffer), bytes / sizeof(int32_t),
              gain);
        break;

      default:
        break;
    }
  }

  void process(const uint8_t* input, size_t bytes) {
    if (input == nullptr || bytes == 0 || channels == 0) {
      return;
    }

    /*
     * VolumeStream also modifies the supplied PCM buffer in-place,
     * so AutomaticGainControlStream follows the same ModifyingStream
     * convention.
     */
    uint8_t* buffer = const_cast<uint8_t*>(input);

    float rms = 0.0;
    float peak = 0.0;

    /*
     * First analyze the ORIGINAL signal.
     *
     * It is important that this happens before applying the new
     * gain.
     */
    if (!analyze(buffer, bytes, rms, peak)) {
      return;
    }

    /*
     * Convert RMS and peak to dBFS.
     */
    float rms_db = linearToDb(static_cast<float>(rms));

    float peak_db = linearToDb(static_cast<float>(peak));

    measured_db = rms_db;
    has_level = true;

    size_t sample_size = bits_per_sample / 8;

    if (sample_size == 0) {
      return;
    }

    size_t samples = bytes / sample_size;

    /*
     * Determine desired gain.
     */
    float desired_gain_db = desiredGainDb(rms_db, peak_db);

    /*
     * Move current gain towards desired gain using asymmetric
     * attack/release smoothing.
     */
    updateGain(desired_gain_db, samples);

    /*
     * Convert dB gain into a linear multiplier.
     */
    float gain = dbToLinear(current_gain_db);

    /*
     * Apply gain to the PCM data.
     */
    apply(buffer, bytes, gain);
  }
};

}  // namespace audio_tools
