#pragma once

#include <math.h>

#include "AudioTools/CoreAudio/Analysis/FrequencyDetectorAutoCorrelation.h"
#include "AudioTools/CoreAudio/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Quality issues detected by QualityAnalysisStream
 * @ingroup dsp
 */
enum class QualityIssue : uint8_t {
  Click,
  Dropout,
  Clipping,
  SignalLost,
  SignalDetected,
};

/**
 * @brief Callback for quality issue notifications
 * @param issue The type of quality issue detected
 * @param count Current total count of this issue type
 */
using QualityCallback = void (*)(QualityIssue issue, uint32_t count);

/**
 * @brief Statistics collected by QualityAnalysisStream
 * @ingroup dsp
 */
struct QualityStats {
  uint32_t click_count = 0;
  uint32_t dropout_count = 0;
  uint32_t clipping_count = 0;
  uint32_t total_samples = 0;

  /// Most recent block RMS level (linear, same scale as the sample values)
  float rms_level = 0.0f;
  /// Slowly tracked noise floor estimate (linear, same scale as rms_level)
  float noise_floor = 0.0f;
  /// Signal-to-noise ratio of the most recent block, in dB
  float snr_db = 0.0f;
  /// Tonality/periodicity confidence (0-1) from the optional autocorrelation
  /// source, see setTonalitySource(). Stays 0 if no source is set.
  float tonality_confidence = 0.0f;
  /// True while rms_level is sustained above the noise floor by the
  /// configured margin (see setSignalMargin()) AND, if a tonality source is
  /// set, the signal is periodic/tonal rather than broadband noise
  bool signal_present = false;
  /// Number of times the signal transitioned from present to lost
  uint32_t signal_lost_count = 0;
  /// Number of times the signal transitioned from lost/absent to present
  uint32_t signal_detected_count = 0;

  void clear() {
    click_count = 0;
    dropout_count = 0;
    clipping_count = 0;
    total_samples = 0;
    rms_level = 0.0f;
    noise_floor = 0.0f;
    snr_db = 0.0f;
    tonality_confidence = 0.0f;
    signal_present = false;
    signal_lost_count = 0;
    signal_detected_count = 0;
  }
};

/**
 * @brief Analyzes audio stream quality by detecting clicks/pops, gaps/dropouts,
 * clipping/corruption, and whether the signal is genuine audio or just noise.
 *
 * Insert this stream into an audio pipeline to monitor signal quality in
 * real time. Data passes through unmodified.
 *
 * - **Clicks/Pops**: detected when the sample-to-sample delta exceeds
 *   a configurable threshold (as a ratio of the maximum sample value).
 * - **Gaps/Dropouts**: detected when consecutive near-zero samples exceed
 *   a configurable minimum length.
 * - **Clipping**: detected when consecutive samples are at (or near) the
 *   maximum representable value.
 * - **Signal vs. noise**: the RMS level is computed over rolling blocks of
 *   samples and used to track a slowly-adapting noise-floor estimate (it
 *   follows the level down quickly but only creeps up slowly, so it isn't
 *   fooled by short loud transients). A block is considered a valid signal
 *   once its RMS level stays above the noise floor by a configurable margin
 *   (setSignalMargin()) for a minimum number of blocks
 *   (setSignalPresenceMinBlocks()). The resulting SNR estimate, noise floor,
 *   RMS level and signal-present flag are all available via stats().
 *
 *   Level alone cannot tell audio content apart from loud broadband noise
 *   (wind, static, hiss) - both simply exceed the noise floor. To also
 *   require the signal to be periodic/tonal (voice, music, tones) rather
 *   than random, this class owns and feeds an internal
 *   FrequencyDetectorAutoCorrelation by default (enabled out of the box);
 *   its normalized autocorrelation confidence is combined with the level
 *   gate before signal_present is raised. Autocorrelation is real CPU cost
 *   (roughly O(sample_rate^2) per second of audio) - on constrained
 *   boards/high sample rates, tune it down with setTonalityBufferSize(), or
 *   turn it off entirely with setTonalityEnabled(false) to fall back to the
 *   level-only gate. Call setTonalitySource() instead if you want to share
 *   an externally-owned/pre-fed detector (e.g. one you already run
 *   elsewhere in the pipeline) rather than the built-in one.
 *
 * Results are available via stats() or through a callback.
 *
 * @ingroup dsp
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class QualityAnalysisStream : public ModifyingStream {
 public:
  QualityAnalysisStream() = default;
  QualityAnalysisStream(Print& print) { setOutput(print); }
  QualityAnalysisStream(Stream& stream) { setStream(stream); }

  bool begin(AudioInfo info) {
    setAudioInfo(info);
    return begin();
  }

  bool begin() override {
    stats_data.clear();
    resetState();
    last_report_time = millis();
    initTonality();
    return ModifyingStream::begin();
  }

  void setAudioInfo(AudioInfo info) override {
    ModifyingStream::setAudioInfo(info);
    max_value = NumberConverter::maxValue(info.bits_per_sample);
    resetState();
    initTonality();
  }

  void setOutput(Print& out) override { p_out = &out; }

  void setStream(Stream& io) override {
    p_out = &io;
    p_io = &io;
  }

  size_t write(const uint8_t* data, size_t len) override {
    analyze(data, len);
    if (p_out != nullptr) return p_out->write(data, len);
    return len;
  }

  size_t readBytes(uint8_t* data, size_t len) override {
    if (p_io == nullptr) return 0;
    size_t result = p_io->readBytes(data, len);
    analyze(data, result);
    return result;
  }

  int available() override {
    return p_io != nullptr ? p_io->available() : 0;
  }

  int availableForWrite() override {
    return p_out != nullptr ? p_out->availableForWrite() : DEFAULT_BUFFER_SIZE;
  }

  void flush() override {
    if (p_out != nullptr) p_out->flush();
  }

  /// Sample-to-sample jump threshold as ratio of max value (0.0 to 1.0)
  void setClickThreshold(float ratio) { click_threshold = ratio; }

  /// Minimum consecutive near-silent samples to count as a dropout
  void setDropoutMinSamples(int samples) { dropout_min_samples = samples; }

  /// Samples below this ratio of max value are considered silent
  void setSilenceThreshold(float ratio) { silence_threshold = ratio; }

  /// Minimum consecutive samples at max to count as clipping
  void setClippingMinSamples(int samples) { clipping_min_samples = samples; }

  /// Clipping is detected when sample >= max_value * (1 - margin)
  void setClippingMargin(float ratio) { clipping_margin = ratio; }

  /// Number of samples per RMS/noise-floor block (default 256)
  void setNoiseFloorBlockSize(int samples) {
    if (samples > 0) rms_block_size = samples;
  }

  /// How quickly the noise floor tracks a falling level (0.0-1.0, default
  /// 0.3). Higher = faster.
  /// How slowly it tracks a rising level (0.0-1.0, default 0.01). Lower =
  /// slower, so brief loud transients don't raise the estimated floor.
  void setNoiseFloorTracking(float attack, float release) {
    noise_floor_attack = attack;
    noise_floor_release = release;
  }

  /// dB the RMS level must stay above the noise floor to count as signal
  /// (default 6dB)
  void setSignalMargin(float db) { signal_margin_db = db; }

  /// Consecutive blocks required above/below the margin before toggling
  /// signal_present (default 3)
  void setSignalPresenceMinBlocks(int blocks) {
    if (blocks > 0) signal_present_min_blocks = blocks;
  }

  /// True while the signal is currently considered present (see
  /// setSignalMargin())
  bool isSignalPresent() const { return stats_data.signal_present; }

  /// Last tonality/periodicity confidence reported by the tonality source
  /// (0 if none is set), see setTonalitySource()
  float tonalityConfidence() const { return stats_data.tonality_confidence; }

  /// Enable/disable the tonality gate (enabled by default using a built-in
  /// FrequencyDetectorAutoCorrelation). When disabled, signal_present depends
  /// only on the level/noise-floor gate. Re-enabling restores the built-in
  /// detector unless an external one was set via setTonalitySource().
  void setTonalityEnabled(bool enabled) {
    tonality_enabled = enabled;
    if (!enabled) {
      p_tonality = nullptr;
    } else if (owns_tonality) {
      auto_tonality.setBufferSize(0);  // force re-derive from current info
      if (info.sample_rate > 0) auto_tonality.begin(info);
      p_tonality = &auto_tonality;
    }
  }

  /// Analysis buffer size (in samples) used by the built-in tonality
  /// detector. Larger = more accurate/expensive, smaller = cheaper/less
  /// accurate. Only relevant while using the built-in detector (i.e. before
  /// calling setTonalitySource()). Applied on the next begin()/setAudioInfo().
  void setTonalityBufferSize(int samples) {
    auto_tonality.setBufferSize(samples);
    if (owns_tonality && tonality_enabled && info.sample_rate > 0) {
      auto_tonality.begin(info);
    }
  }

  /// Minimum tonality confidence (0.0-1.0, default 0.3) for a block to count
  /// as signal rather than noise.
  void setTonalityMinConfidence(float min_confidence) {
    tonality_min_confidence = min_confidence;
  }

  /// Overrides the built-in tonality detector with an externally-owned one
  /// that you feed yourself (e.g. because you already run it elsewhere in
  /// the pipeline). You are responsible for calling begin()/write() on it;
  /// this class only reads its confidence().
  /// @param detector The autocorrelation detector to query.
  /// @param min_confidence Minimum tonality confidence (0.0-1.0, default 0.3).
  /// @param channel Channel to query on the detector (default 0).
  void setTonalitySource(FrequencyDetectorAutoCorrelation& detector,
                          float min_confidence = 0.3f, int channel = 0) {
    owns_tonality = false;
    tonality_enabled = true;
    p_tonality = &detector;
    tonality_min_confidence = min_confidence;
    tonality_channel = channel;
  }

  /// Reverts to the built-in tonality detector (the default), undoing a
  /// prior setTonalitySource().
  void clearTonalitySource() {
    owns_tonality = true;
    initTonality();
  }

  /// Enable periodic reporting of quality KPIs
  /// @param period_ms Reporting interval in milliseconds
  /// @param output Print target (e.g. Serial)
  void setReporting(int period_ms, Print& output) {
    report_period_ms = period_ms;
    p_report = &output;
  }

  /// Register a callback for quality issue notifications
  void setCallback(QualityCallback cb) { callback = cb; }

  /// Access the accumulated quality statistics
  const QualityStats& stats() const { return stats_data; }

  /// Reset all statistics and detection state
  void clearStats() {
    stats_data.clear();
    resetState();
  }

 protected:
  Print* p_out = nullptr;
  Stream* p_io = nullptr;

  QualityStats stats_data;
  QualityCallback callback = nullptr;

  Print* p_report = nullptr;
  int report_period_ms = 0;
  uint32_t last_report_time = 0;

  float click_threshold = 0.5f;
  int dropout_min_samples = 10;
  float silence_threshold = 0.01f;
  int clipping_min_samples = 3;
  float clipping_margin = 0.01f;

  int rms_block_size = 256;
  float noise_floor_attack = 0.3f;
  float noise_floor_release = 0.01f;
  float signal_margin_db = 6.0f;
  int signal_present_min_blocks = 3;
  FrequencyDetectorAutoCorrelation auto_tonality;
  FrequencyDetectorAutoCorrelation* p_tonality = nullptr;
  bool tonality_enabled = true;
  bool owns_tonality = true;
  float tonality_min_confidence = 0.3f;
  int tonality_channel = 0;

  float max_value = 32767.0f;
  // per-channel previous sample for click detection
  Vector<float> prev_sample;
  bool has_prev_sample = false;
  // dropout detection
  int consecutive_silent = 0;
  bool in_dropout = false;
  // clipping detection
  int consecutive_clipped = 0;
  bool in_clipping = false;
  // signal vs. noise detection
  double rms_sum_sq = 0;
  int rms_block_count = 0;
  bool noise_floor_initialized = false;
  int consecutive_signal_blocks = 0;
  int consecutive_noise_blocks = 0;

  void resetState() {
    int ch = info.channels > 0 ? info.channels : 1;
    prev_sample.resize(ch);
    for (int i = 0; i < ch; i++) prev_sample[i] = 0;
    has_prev_sample = false;
    consecutive_silent = 0;
    in_dropout = false;
    consecutive_clipped = 0;
    in_clipping = false;
    rms_sum_sq = 0;
    rms_block_count = 0;
    noise_floor_initialized = false;
    consecutive_signal_blocks = 0;
    consecutive_noise_blocks = 0;
  }

  /// (Re-)initializes the built-in tonality detector to match the current
  /// AudioInfo, unless an external one was set via setTonalitySource() or
  /// the tonality gate was disabled via setTonalityEnabled(false).
  void initTonality() {
    if (!tonality_enabled) {
      p_tonality = nullptr;
      return;
    }
    if (!owns_tonality) return;  // external detector managed by the caller
    if (info.sample_rate <= 0) return;
    auto_tonality.setBufferSize(0);  // re-derive buffer size from info
    auto_tonality.begin(info);
    p_tonality = &auto_tonality;
  }

  void reportIfDue() {
    if (p_report == nullptr || report_period_ms <= 0) return;
    uint32_t now = millis();
    if (now - last_report_time >= (uint32_t)report_period_ms) {
      last_report_time = now;
      printReport();
    }
  }

  void printReport() {
    char msg[220];
    if (p_tonality != nullptr) {
      snprintf(msg, sizeof(msg),
               "Quality: clicks=%lu, dropouts=%lu, clipping=%lu, samples=%lu, "
               "rms=%.1f, noise_floor=%.1f, snr=%.1fdB, tonality=%.2f, signal=%s",
               (unsigned long)stats_data.click_count,
               (unsigned long)stats_data.dropout_count,
               (unsigned long)stats_data.clipping_count,
               (unsigned long)stats_data.total_samples,
               (double)stats_data.rms_level, (double)stats_data.noise_floor,
               (double)stats_data.snr_db, (double)stats_data.tonality_confidence,
               stats_data.signal_present ? "yes" : "no");
    } else {
      snprintf(msg, sizeof(msg),
               "Quality: clicks=%lu, dropouts=%lu, clipping=%lu, samples=%lu, "
               "rms=%.1f, noise_floor=%.1f, snr=%.1fdB, signal=%s",
               (unsigned long)stats_data.click_count,
               (unsigned long)stats_data.dropout_count,
               (unsigned long)stats_data.clipping_count,
               (unsigned long)stats_data.total_samples,
               (double)stats_data.rms_level, (double)stats_data.noise_floor,
               (double)stats_data.snr_db,
               stats_data.signal_present ? "yes" : "no");
    }
    p_report->println(msg);
  }

  void analyze(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) return;
    // Feed the built-in tonality detector with the same raw audio (an
    // externally-owned one set via setTonalitySource() is fed by the caller)
    if (owns_tonality && tonality_enabled && p_tonality != nullptr) {
      p_tonality->write(data, len);
    }
    switch (info.bits_per_sample) {
      case 8:
        analyzeT<int8_t>(data, len);
        break;
      case 16:
        analyzeT<int16_t>(data, len);
        break;
      case 24:
        analyzeT<int24_t>(data, len);
        break;
      case 32:
        analyzeT<int32_t>(data, len);
        break;
      default:
        LOGE("QualityAnalysisStream: unsupported bits_per_sample %d",
             info.bits_per_sample);
        break;
    }
    reportIfDue();
  }

  template <typename T>
  void analyzeT(const uint8_t* buffer, size_t size) {
    T* samples = (T*)buffer;
    int count = size / sizeof(T);
    int channels = info.channels > 0 ? info.channels : 1;
    float abs_click = click_threshold * max_value;
    float abs_silence = silence_threshold * max_value;
    float abs_clip = max_value * (1.0f - clipping_margin);

    for (int i = 0; i < count; i++) {
      float val = static_cast<float>(static_cast<int>(samples[i]));
      float abs_val = val < 0 ? -val : val;
      int ch = i % channels;

      stats_data.total_samples++;

      // Click / pop detection
      if (has_prev_sample) {
        float delta = val - prev_sample[ch];
        if (delta < 0) delta = -delta;
        if (delta > abs_click) {
          stats_data.click_count++;
          if (callback) callback(QualityIssue::Click, stats_data.click_count);
        }
      }
      prev_sample[ch] = val;

      // Dropout detection
      if (abs_val <= abs_silence) {
        consecutive_silent++;
        if (!in_dropout && consecutive_silent >= dropout_min_samples) {
          in_dropout = true;
          stats_data.dropout_count++;
          if (callback)
            callback(QualityIssue::Dropout, stats_data.dropout_count);
        }
      } else {
        consecutive_silent = 0;
        in_dropout = false;
      }

      // Clipping detection
      if (abs_val >= abs_clip) {
        consecutive_clipped++;
        if (!in_clipping && consecutive_clipped >= clipping_min_samples) {
          in_clipping = true;
          stats_data.clipping_count++;
          if (callback)
            callback(QualityIssue::Clipping, stats_data.clipping_count);
        }
      } else {
        consecutive_clipped = 0;
        in_clipping = false;
      }

      // Signal vs. noise (RMS / noise-floor) detection
      rms_sum_sq += (double)val * (double)val;
      rms_block_count++;
      if (rms_block_count >= rms_block_size) {
        processRmsBlock();
      }
    }

    // Mark that we have valid previous samples after the first buffer
    if (count >= channels) has_prev_sample = true;
  }

  /// Called once per rms_block_size samples: updates rms_level, noise_floor,
  /// snr_db and the signal_present flag/callbacks.
  void processRmsBlock() {
    float block_rms = sqrt((float)(rms_sum_sq / rms_block_count));
    rms_sum_sq = 0;
    rms_block_count = 0;

    if (!noise_floor_initialized) {
      stats_data.noise_floor = block_rms;
      noise_floor_initialized = true;
    } else if (block_rms < stats_data.noise_floor) {
      stats_data.noise_floor +=
          (block_rms - stats_data.noise_floor) * noise_floor_attack;
    } else {
      stats_data.noise_floor +=
          (block_rms - stats_data.noise_floor) * noise_floor_release;
    }

    stats_data.rms_level = block_rms;
    float floor = stats_data.noise_floor > 1.0f ? stats_data.noise_floor : 1.0f;
    float level = block_rms > 1.0f ? block_rms : 1.0f;
    stats_data.snr_db = 20.0f * log10(level / floor);

    if (p_tonality != nullptr) {
      stats_data.tonality_confidence = p_tonality->confidence(tonality_channel);
    }

    float threshold = stats_data.noise_floor * pow(10.0f, signal_margin_db / 20.0f);
    bool level_ok = block_rms > threshold;
    bool tonal_ok = p_tonality == nullptr ||
                    stats_data.tonality_confidence >= tonality_min_confidence;
    if (level_ok && tonal_ok) {
      consecutive_signal_blocks++;
      consecutive_noise_blocks = 0;
      if (!stats_data.signal_present &&
          consecutive_signal_blocks >= signal_present_min_blocks) {
        stats_data.signal_present = true;
        stats_data.signal_detected_count++;
        if (callback)
          callback(QualityIssue::SignalDetected,
                    stats_data.signal_detected_count);
      }
    } else {
      consecutive_noise_blocks++;
      consecutive_signal_blocks = 0;
      if (stats_data.signal_present &&
          consecutive_noise_blocks >= signal_present_min_blocks) {
        stats_data.signal_present = false;
        stats_data.signal_lost_count++;
        if (callback)
          callback(QualityIssue::SignalLost, stats_data.signal_lost_count);
      }
    }
  }
};

}  // namespace audio_tools
