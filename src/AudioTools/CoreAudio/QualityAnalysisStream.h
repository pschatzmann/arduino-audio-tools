#pragma once

#include "AudioStreams.h"

namespace audio_tools {

/**
 * @brief Quality issues detected by QualityAnalysisStream
 * @ingroup dsp
 */
enum class QualityIssue : uint8_t {
  Click,
  Dropout,
  Clipping,
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

  void clear() {
    click_count = 0;
    dropout_count = 0;
    clipping_count = 0;
    total_samples = 0;
  }
};

/**
 * @brief Analyzes audio stream quality by detecting clicks/pops, gaps/dropouts,
 * and clipping/corruption.
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
    return ModifyingStream::begin();
  }

  void setAudioInfo(AudioInfo info) override {
    ModifyingStream::setAudioInfo(info);
    max_value = NumberConverter::maxValue(info.bits_per_sample);
    resetState();
  }

  void setOutput(Print& out) override { p_out = &out; }

  void setStream(Stream& io) override {
    p_out = &io;
    p_stream = &io;
  }

  void setOutput(AudioOutput& out) {
    addNotifyAudioChange(out);
    setOutput((Print&)out);
  }

  void setStream(AudioStream& io) {
    addNotifyAudioChange(io);
    setStream((Stream&)io);
  }

  size_t write(const uint8_t* data, size_t len) override {
    analyze(data, len);
    if (p_out != nullptr) return p_out->write(data, len);
    return len;
  }

  size_t readBytes(uint8_t* data, size_t len) override {
    if (p_stream == nullptr) return 0;
    size_t result = p_stream->readBytes(data, len);
    analyze(data, result);
    return result;
  }

  int available() override {
    return p_stream != nullptr ? p_stream->available() : 0;
  }

  int availableForWrite() override {
    return p_out != nullptr ? p_out->availableForWrite() : DEFAULT_BUFFER_SIZE;
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
  Stream* p_stream = nullptr;

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

  void resetState() {
    int ch = info.channels > 0 ? info.channels : 1;
    prev_sample.resize(ch);
    for (int i = 0; i < ch; i++) prev_sample[i] = 0;
    has_prev_sample = false;
    consecutive_silent = 0;
    in_dropout = false;
    consecutive_clipped = 0;
    in_clipping = false;
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
    char msg[120];
    snprintf(msg, sizeof(msg),
             "Quality: clicks=%lu, dropouts=%lu, clipping=%lu, samples=%lu",
             (unsigned long)stats_data.click_count,
             (unsigned long)stats_data.dropout_count,
             (unsigned long)stats_data.clipping_count,
             (unsigned long)stats_data.total_samples);
    p_report->println(msg);
  }

  void analyze(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) return;
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
    }

    // Mark that we have valid previous samples after the first buffer
    if (count >= channels) has_prev_sample = true;
  }
};

}  // namespace audio_tools
