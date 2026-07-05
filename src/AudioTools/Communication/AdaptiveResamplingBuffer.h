#pragma once

#include <atomic>

#include "AudioTools/CoreAudio/AudioBasic/KalmanFilter.h"
#include "AudioTools/CoreAudio/AudioBasic/PIDController.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/ResampleStream.h"

namespace audio_tools {

/**
 * @brief A BaseBuffer<uint8_t> that wraps a raw (unresampled) backing buffer
 * and transparently resamples on read to correct jitter and compensate for
 * the slightly different clock rates between an audio source and audio
 * target. Use separate tasks to write and read the data. Also make sure that
 * you protect the access with a mutex or provide a thread-safe backing
 * buffer!
 *
 * The resampling step size combines a feedforward term - the long-run
 * average step size implied by cumulative bytes written vs. bytes read,
 * see averageStepSize() - with feedback from a PID controller reacting to
 * the (Kalman-filter-smoothed) instantaneous buffer fill level. The
 * feedforward term absorbs genuine long-term clock drift; the PID only
 * needs to correct short-term jitter around it, which is considerably
 * more stable than relying on the PID alone.
 *
 * The calculated step size (this class's own copy, read via stepSize() and
 * used by available()) is a std::atomic<float>, since it's written by the
 * writer task (in writeArray()/recalculate()) and read by the reader task
 * (in available()) - this rules out torn reads on any platform, including
 * 8-bit targets like AVR where a 32-bit float load/store isn't a single
 * bus transaction.
 *
 * Note this only covers this class's own copy of the value. The copy
 * actually consumed on the hot path - ResampleStream's internal step size,
 * read on every interpolation iteration inside readArray() - is a plain
 * (non-atomic) float, since ResampleStream is a general-purpose, usually
 * single-threaded class and making its hot-path field atomic would add a
 * memory-barrier cost for all its other users too. In practice this is a
 * low-risk, deliberate trade-off: on 32-bit targets (ESP32, RP2040, ARM)
 * an aligned float load/store is already a single atomic bus transaction
 * in hardware, so a torn read there is not possible; only 8-bit targets
 * are theoretically exposed, and a torn read there would at worst cause
 * one glitchy resample step, never a crash.
 *
 * @ingroup buffers
 * @ingroup communications
 * @author Phil Schatzmann
 */
class AdaptiveResamplingBuffer : public BaseBuffer<uint8_t>,
                                  public AudioInfoSupport {
 public:
  /**
   * @brief Construct a new AdaptiveResamplingBuffer object
   * You need to call setBuffer() and setStepRangePercent() before begin()
   */
  AdaptiveResamplingBuffer() = default;

  /**
   * @brief Construct a new AdaptiveResamplingBuffer object
   *
   * @param buffer Reference to the raw (unresampled) backing buffer
   * @param stepRangePercent Allowed resampling range in percent (default: 5.0)
   */
  AdaptiveResamplingBuffer(BaseBuffer<uint8_t>& buffer,
                            float stepRangePercent = 5.0f) {
    setBuffer(buffer);
    setStepRangePercent(stepRangePercent);
  }

  /**
   * @brief Defines the fill level (in percent of the backing buffer's
   * capacity) that must be reached once, right after begin(), before any
   * data is provided to the reader. This avoids starting playback only to
   * immediately underrun again. Defaults to 50%. Priming only happens once
   * at the start of a session (i.e. after begin()/reset()); it does not
   * re-arm on a later underflow.
   *
   * @param percent Fill level in percent (e.g. 50.0 for 50%); 0 disables
   * priming.
   */
  void setStartFillPercent(float percent) { start_fill_percent = percent; }

  // resample_stream internally stores a pointer to the sibling
  // queue_stream member (via setStream()). A copy would leave the copy's
  // resample_stream pointing at the original's queue_stream, silently
  // sharing/dangling state - so copying (and, since neither QueueStream
  // nor ResampleStream defines a real move, moving) is disabled.
  AdaptiveResamplingBuffer(const AdaptiveResamplingBuffer&) = delete;
  AdaptiveResamplingBuffer& operator=(const AdaptiveResamplingBuffer&) = delete;

  /**
   * @brief Set the raw (unresampled) backing buffer
   *
   * @param buffer Reference to the raw (unresampled) backing buffer
   */
  void setBuffer(BaseBuffer<uint8_t>& buffer) { p_buffer = &buffer; }

  /**
   * @brief Defines the audio format: needed to correctly interpret and
   * resample the sample frames stored in the backing buffer.
   */
  void setAudioInfo(AudioInfo info) override {
    audio_info = info;
    // if we are already running, apply the change immediately
    if (is_active) resample_stream.setAudioInfo(audio_info);
  }

  AudioInfo audioInfo() override { return audio_info; }

  /**
   * @brief Initialize the buffer and internal components.
   *
   * @return true if initialization was successful, false otherwise
   */
  bool begin() {
    if (p_buffer == nullptr) return false;
    if (resample_range <= 0.0f) {
      LOGW(
          "resample_range is 0: call setStepRangePercent() before begin() "
          "to enable adaptive resampling");
    }
    queue_stream.setBuffer(*p_buffer);
    queue_stream.begin();
    resample_stream.setStream(queue_stream);
    resample_stream.begin(audio_info);
    // This is a jitter buffer for an endless/live stream: a momentarily
    // empty backing buffer is a transient underflow, not end of stream.
    resample_stream.transformationReader().setEofOnZeroReads(false);
    last_time_ms = 0;
    is_active = true;
    is_primed = false;
    resetDriftCounters();
    // pid.calculate() returns a correction in [-resample_range, +resample_range]
    // which is subtracted from the feedforward center in recalculate().
    return pid.begin(1.0, resample_range, -resample_range, p, i, d);
  }

  /**
   * @brief End the buffer and release resources.
   */
  void end() {
    is_active = false;
    queue_stream.end();
    resample_stream.end();
    recalc_count = 0;
    last_time_ms = 0;
    has_peek = false;
  }

  /// Writes a single byte to the backing buffer.
  bool write(uint8_t data) override { return writeArray(&data, 1) == 1; }

  /// Writes multiple bytes to the backing buffer and triggers a step size
  /// recalculation.
  int writeArray(const uint8_t data[], int len) override {
    if (p_buffer == nullptr) return 0;
    int result = p_buffer->writeArray(data, len);
    total_bytes_written += result;
    recalculate();
    return result;
  }

  /// Reads a single resampled byte.
  bool read(uint8_t& result) override { return readArray(&result, 1) == 1; }

  /// Reads multiple resampled bytes. Returns 0 while priming (see
  /// setStartFillPercent()) even if the backing buffer already has data.
  int readArray(uint8_t data[], int len) override {
    if (p_buffer == nullptr || len <= 0 || !checkPrimed()) return 0;
    int written = 0;
    if (has_peek) {
      data[written++] = peek_byte;
      has_peek = false;
    }
    if (written < len) {
      written += resample_stream.readBytes(data + written, len - written);
    }
    total_bytes_read += written;
    return written;
  }

  /// Peeks the next resampled byte without consuming it. Returns false
  /// while priming (see setStartFillPercent()).
  bool peek(uint8_t& result) override {
    if (!checkPrimed()) return false;
    if (!has_peek) {
      if (resample_stream.readBytes(&peek_byte, 1) != 1) return false;
      has_peek = true;
    }
    result = peek_byte;
    return true;
  }

  /// Clears the backing buffer and reinitializes the resampling pipeline
  /// (interpolation history, queued/lookahead bytes, PID and Kalman filter
  /// state, step size) so nothing from before the reset leaks into the
  /// next session.
  void reset() override {
    if (p_buffer != nullptr) p_buffer->reset();
    has_peek = false;
    pid.reset();
    kalman_filter.end();
    step_size = 1.0f;
    level_percent_smoothed = 0.0f;
    last_time_ms = 0;
    is_primed = false;
    resetDriftCounters();
    if (is_active) {
      resample_stream.end();
      resample_stream.begin(audio_info);
      resample_stream.setStepSize(step_size);
    }
  }

  /// Estimated number of resampled bytes currently available to read: the
  /// raw backing buffer's fill scaled by the current step size. This is an
  /// estimate, not an exact promise - actual interpolation/frame alignment
  /// in readArray() can return slightly more or less - but it's a cheap,
  /// side-effect-free query, unlike actually running the resampler ahead of
  /// time just to count bytes (which would itself drain the backing buffer
  /// and distort the PID's fill-level feedback - see recalculate()).
  int available() override {
    if (p_buffer == nullptr || !checkPrimed()) return 0;
    int raw_available = p_buffer->available();
    float estimate = step_size > 0.0f ? raw_available / step_size : raw_available;
    return (has_peek ? 1 : 0) + (int)estimate;
  }

  /// Number of raw bytes that can still be written to the backing buffer.
  int availableForWrite() override {
    if (p_buffer == nullptr) return 0;
    return p_buffer->availableForWrite();
  }

  /// Not supported: resampled data has no contiguous physical representation.
  uint8_t* address() override { return nullptr; }

  /// Capacity (in bytes) of the backing buffer.
  size_t size() override { return p_buffer == nullptr ? 0 : p_buffer->size(); }

  /// Current actual fill level of the backing buffer in percent (0-100).
  float levelPercent() override {
    if (p_buffer == nullptr) return 0.0f;
    return p_buffer->levelPercent();
  }

  /**
   * @brief Recalculate the resampling step size based on buffer fill level.
   * Called automatically by writeArray().
   *
   * @return float The new step size
   */
  float recalculate() {
    if (p_buffer == nullptr) return step_size;

    // determine the actual elapsed time so the PID's integral/derivative
    // terms don't depend on how often/how large the caller's writes are
    uint32_t now_ms = millis();
    float dt = last_time_ms == 0 ? 1.0f : (now_ms - last_time_ms) / 1000.0f;
    if (dt <= 0.0f) dt = 0.001f;
    pid.setDt(dt);
    last_time_ms = now_ms;

    // calculate new resampling step size
    level_percent_smoothed = p_buffer->levelPercent();
    kalman_filter.addMeasurement(level_percent_smoothed);
    // A larger step size makes the resampler consume the buffered input
    // faster, so a low fill level (correction > 0) must reduce the step
    // size, and a high fill level (correction < 0) must increase it.
    float correction = pid.calculate(50.0, kalman_filter.calculate());
    // Feedforward + feedback: center on the long-run average step size
    // implied by cumulative bytes written vs. read (a low-noise estimate
    // of genuine producer/consumer clock drift) and let the PID's
    // correction only handle short-term jitter around that center. This
    // is far more stable than centering on a fixed 1.0 and relying solely
    // on the PID to chase both drift and jitter from a noisy instantaneous
    // fill level.
    step_size = averageStepSize() - correction;

    // log step size every 100th recalculation
    if (recalc_count++ % 100 == 0) {
      LOGI("step_size: %f", step_size.load());
    }

    resample_stream.setStepSize(step_size);
    return step_size;
  }

  /**
   * @brief Long-run average step size implied by cumulative bytes written
   * vs. bytes read, used as the feedforward center for recalculate(). This
   * converges to the true average clock-drift ratio between producer and
   * consumer and gets less noisy the longer the stream runs, unlike the
   * instantaneous buffer fill level. Returns 1.0 until enough bytes have
   * flowed to make the ratio meaningful (see setMinBytesForDriftEstimate()).
   *
   * @return float Estimated average step size
   */
  float averageStepSize() {
    if (total_bytes_read < min_bytes_for_drift_estimate) return 1.0f;
    // Deliberately NOT netted against the buffer's current occupancy
    // (e.g. total_written - p_buffer->available()): that quantity is
    // itself the result of whatever step size the resampler already
    // applied, so feeding it back as the next step size's center creates
    // a circular, self-reinforcing loop - any startup transient gets
    // permanently amplified instead of forgotten. total_bytes_written and
    // total_bytes_read are each driven purely by the source/sink and
    // don't depend on our own past control output, so their ratio is the
    // step size that would have kept the buffer's fill level unchanged
    // over the window - a genuine, non-circular drift estimate. Residual
    // absolute fill-level error is exactly what the PID feedback term
    // already corrects for.
    float ratio = (float)total_bytes_written / (float)total_bytes_read;
    // safety net against a pathological/runaway estimate; real clock drift
    // is normally well under 1%, so this only guards against bugs/edge cases
    if (ratio < 0.5f) ratio = 0.5f;
    if (ratio > 2.0f) ratio = 2.0f;
    return ratio;
  }

  /**
   * @brief Minimum cumulative output bytes that must have been read before
   * averageStepSize() is trusted; below this it returns 1.0. Avoids acting
   * on a ratio computed from a handful of bytes right after begin().
   * Default is 4096.
   */
  void setMinBytesForDriftEstimate(uint32_t bytes) {
    min_bytes_for_drift_estimate = bytes;
  }

  /// Cumulative raw bytes written to the backing buffer since the drift
  /// counters were last reset (begin(), reset(), or priming completion).
  uint64_t totalBytesWritten() { return total_bytes_written; }

  /// Cumulative resampled bytes delivered via readArray() since the drift
  /// counters were last reset (begin(), reset(), or priming completion).
  uint64_t totalBytesRead() { return total_bytes_read; }

  /**
   * @brief Set the allowed resampling range as a percent.
   *
   * @param rangePercent Allowed range in percent (e.g., 5.0 for ± 5%)
   */
  void setStepRangePercent(float rangePercent) {
    resample_range = rangePercent / 100.0;
  }

  /**
   * @brief Get the Kalman-smoothed fill level from the last recalculate()
   * call, in percent.
   *
   * @return float Last calculated (smoothed) fill level (0-100)
   */
  float levelPercentSmoothed() { return level_percent_smoothed; }

  /// Get the resampling step size calculated by the last recalculate()
  /// call, without triggering a new calculation.
  float stepSize() { return step_size; }

  /// True once the buffer has reached setStartFillPercent() (or always
  /// true if priming is disabled).
  bool isPrimed() { return checkPrimed(); }

  /**
   * @brief Set the Kalman filter parameters.
   *
   * @param process_noise Process noise covariance (Q)
   * @param measurement_noise Measurement noise covariance (R)
   */
  void setKalmanParameters(float process_noise, float measurement_noise) {
    kalman_filter.begin(process_noise, measurement_noise);
  }

  /**
   * @brief Set the PID controller parameters.
   *
   * @param p_value Proportional gain
   * @param i_value Integral gain
   * @param d_value Derivative gain
   */
  void setPIDParameters(float p_value, float i_value, float d_value) {
    p = p_value;
    i = i_value;
    d = d_value;
  }

 protected:
  /// Resets the cumulative counters used by averageStepSize().
  void resetDriftCounters() {
    total_bytes_written = 0;
    total_bytes_read = 0;
  }

  /// Returns true once priming is complete; latches permanently once the
  /// fill threshold has been reached (does not re-arm on later underflow).
  bool checkPrimed() {
    if (is_primed) return true;
    if (start_fill_percent <= 0.0f) {
      is_primed = true;
    } else if (p_buffer != nullptr &&
               p_buffer->levelPercent() >= start_fill_percent) {
      is_primed = true;
    }
    if (is_primed) {
      // Priming accumulates writes with no matching reads, so the
      // resampler's first-ever read would have to work through that
      // whole backlog at once - permanently skewing the cumulative
      // written-vs-read ratio used by averageStepSize(). Start counting
      // only from the moment real (post-priming) traffic begins.
      resetDriftCounters();
    }
    return is_primed;
  }

  PIDController pid;                // PID controller for adaptive resampling step size
  QueueStream<uint8_t> queue_stream; // Internal queue stream for buffering audio data
  BaseBuffer<uint8_t>* p_buffer = nullptr; // Pointer to the user-provided raw buffer
  ResampleStream resample_stream;   // Resample stream for adjusting playback rate
  KalmanFilter kalman_filter{0.01f, 0.1f}; // Kalman filter for smoothing buffer fill level
  AudioInfo audio_info;              // Audio format of the buffered data
  std::atomic<float> step_size{1.0f}; // Current resampling step size (see class docs on thread-safety)
  float resample_range = 0;         // Allowed resampling range (fraction)
  float p = 0.005;                  // PID proportional gain
  float i = 0.00005;                // PID integral gain
  float d = 0.0001;                 // PID derivative gain
  float level_percent_smoothed = 0.0; // Last calculated (Kalman-smoothed) fill level (percent)
  uint32_t recalc_count = 0;        // recalculate() call counter (used to throttle logging)
  uint32_t last_time_ms = 0;        // Timestamp of the last recalculate() call, for PID dt
  bool is_active = false;           // true between begin() and end()
  uint8_t peek_byte = 0;            // one-byte lookahead cache for peek()
  bool has_peek = false;            // true if peek_byte holds an unconsumed byte
  float start_fill_percent = 50.0f; // required fill level before priming completes (0 = disabled)
  bool is_primed = false;           // true once the start fill level has been reached once
  uint64_t total_bytes_written = 0; // cumulative raw bytes written since counters were last reset
  uint64_t total_bytes_read = 0;    // cumulative resampled bytes delivered since counters were last reset
  uint32_t min_bytes_for_drift_estimate = 4096; // warm-up threshold for averageStepSize()
};

}  // namespace audio_tools
