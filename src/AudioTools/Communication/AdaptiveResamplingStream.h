#pragma once

#include "AudioTools/CoreAudio/AudioBasic/KalmanFilter.h"
#include "AudioTools/CoreAudio/AudioBasic/PIDController.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/ResampleStream.h"

namespace audio_tools {

/**
 * @brief An Audio Stream backed by a buffer (queue) which tries to correct
 * jitter and automatically adjusts for the slightly different clock rates
 * between an audio source and audio target. Use separate tasks to write and
 * read the data. Also make sure that you protect the access with a mutex or
 * provide a thread-safe buffer!
 *
 * The resamping step size is calculated with the help of a PID controller.
 * The fill level is smoothed using a Kalman filter.
 *
 * @ingroup buffers
 * @ingroup communications
 * @author Phil Schatzmann
 */
class AdaptiveResamplingStream : public AudioStream {
 public:
  /**
   * @brief Construct a new AdaptiveResamplingStream object
   *
   * @param buffer Reference to the buffer used for audio data
   * @param stepRangePercent Allowed resampling range in percent (default: 0.05)
   */
  AdaptiveResamplingStream(BaseBuffer<uint8_t>& buffer,
                           float stepRangePercent = 5.0f) {
    p_buffer = &buffer;
    setStepRangePercent(stepRangePercent);
    addNotifyAudioChange(resample_stream);
  }

  /**
   * @brief Initialize the stream and internal components.
   *
   * @return true if initialization was successful, false otherwise
   */
  bool begin() {
    if (p_buffer == nullptr) return false;
    queue_stream.setBuffer(*p_buffer);
    queue_stream.begin();
    resample_stream.setAudioInfo(audioInfo());
    resample_stream.setStream(queue_stream);
    resample_stream.begin(audioInfo());
    float from_step = 1.0 - resample_range;
    float to_step = 1.0 + resample_range;
    return pid.begin(1.0, from_step, to_step, p, i, d);
  }

  /**
   * @brief Write audio data to the buffer.
   *
   * @param data Pointer to the data to write
   * @param len Number of bytes to write
   * @return size_t Number of bytes actually written
   */
  size_t write(const uint8_t* data, size_t len) override {
    if (p_buffer == nullptr) return 0;
    size_t result = p_buffer->writeArray(data, len);
    recalculate();
    return result;
  }

  /**
   * @brief End the stream and release resources.
   */
  void end() {
    queue_stream.end();
    resample_stream.end();
    read_count = 0;
  }

  /**
   * @brief Read resampled audio data from the buffer.
   *
   * @param data Pointer to the buffer to fill
   * @param len Number of bytes to read
   * @return size_t Number of bytes actually read
   */
  size_t readBytes(uint8_t* data, size_t len) override {
    if (p_buffer == nullptr) return 0;

    return resample_stream.readBytes(data, len);
  }

  /**
   * @brief Recalculate the resampling step size based on buffer fill level.
   *
   * @return float The new step size
   */
  float recalculate() {
    if (p_buffer == nullptr) return step_size;

    // calculate new resampling step size
    level_percent = p_buffer->levelPercent();
    kalman_filter.addMeasurement(level_percent);
    step_size = pid.calculate(50.0, kalman_filter.calculate());

    // log step size every 100th read
    if (read_count++ % 100 == 0) {
      LOGI("step_size: %f", step_size);
    }

    // return resampled result
    resample_stream.setStepSize(step_size);
    return step_size;
  }

  /**
   * @brief Set the allowed resampling range as a percent.
   *
   * @param rangePercent Allowed range in percent (e.g., 5.0 for ± 5%)
   */
  void setStepRangePercent(float rangePercent) {
    resample_range = rangePercent / 100.0;
  }

  /**
   * @brief Get the current actual buffer fill level in percent.
   *
   * @return float Current fill level (0-100)
   */
  float levelPercentActual() {
    if (p_buffer == nullptr) return 0.0f;
    return p_buffer->levelPercent();
  }

  /**
   * @brief Get the fill level at the last calculation in percent.
   *
   * @return float Last calculated fill level (0-100)
   */
  float levelPercent() { return level_percent; }

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
  PIDController pid;                // PID controller for adaptive resampling step size (p=0.005, i=0.00005, d=0.0001)
  QueueStream<uint8_t> queue_stream; // Internal queue stream for buffering audio data
  BaseBuffer<uint8_t>* p_buffer = nullptr; // Pointer to the user-provided buffer
  ResampleStream resample_stream;   // Resample stream for adjusting playback rate
  KalmanFilter kalman_filter{0.01f, 0.1f}; // Kalman filter for smoothing buffer fill level
  float step_size = 1.0;            // Current resampling step size
  float resample_range = 0;         // Allowed resampling range (fraction)
  float p = 0.005;                  // PID proportional gain
  float i = 0.00005;                // PID integral gain
  float d = 0.0001;                 // PID derivative gain
  float level_percent = 0.0;        // Last calculated buffer fill level (percent)
  uint32_t read_count = 0;          // Read operation counter
};

}  // namespace audio_tools
