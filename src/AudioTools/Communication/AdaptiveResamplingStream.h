#pragma once

#include "AudioTools/Communication/AdaptiveResamplingBuffer.h"
#include "AudioTools/CoreAudio/AudioStreams.h"

namespace audio_tools {

/**
 * @brief An Audio Stream backed by a buffer (queue) which tries to correct
 * jitter and automatically adjusts for the slightly different clock rates
 * between an audio source and audio target. Use separate tasks to write and
 * read the data. Also make sure that you protect the access with a mutex or
 * provide a thread-safe buffer!
 *
 * This is a thin Stream adapter around AdaptiveResamplingBuffer, which
 * implements the actual jitter-buffering/resampling logic behind the
 * BaseBuffer<uint8_t> API. If you need a BaseBuffer (e.g. to plug into an
 * API that expects one) rather than a Stream, use AdaptiveResamplingBuffer
 * directly.
 *
 * @ingroup buffers
 * @ingroup communications
 * @author Phil Schatzmann
 */
class AdaptiveResamplingStream : public AudioStream {
 public:
  /**
   * @brief Construct a new AdaptiveResamplingStream object
   * You need to call setBuffer() and setStepRangePercent() before begin()
   */
  AdaptiveResamplingStream() { addNotifyAudioChange(resampling_buffer); }
  /**
   * @brief Construct a new AdaptiveResamplingStream object
   *
   * @param buffer Reference to the buffer used for audio data
   * @param stepRangePercent Allowed resampling range in percent (default: 0.05)
   */
  AdaptiveResamplingStream(BaseBuffer<uint8_t>& buffer,
                           float stepRangePercent = 5.0f)
      : resampling_buffer(buffer, stepRangePercent) {
    addNotifyAudioChange(resampling_buffer);
  }

  /**
   * @brief Set the buffer used for audio data
   *
   * @param buffer Reference to the buffer used for audio data
   */
  void setBuffer(BaseBuffer<uint8_t>& buffer) {
    resampling_buffer.setBuffer(buffer);
  }

  /**
   * @brief Initialize the stream and internal components.
   *
   * @return true if initialization was successful, false otherwise
   */
  bool begin() {
    resampling_buffer.setAudioInfo(audioInfo());
    return resampling_buffer.begin();
  }

  /**
   * @brief Write audio data to the buffer.
   *
   * @param data Pointer to the data to write
   * @param len Number of bytes to write
   * @return size_t Number of bytes actually written
   */
  size_t write(const uint8_t* data, size_t len) override {
    return resampling_buffer.writeArray(data, len);
  }

  /**
   * @brief End the stream and release resources.
   */
  void end() { resampling_buffer.end(); }

  /**
   * @brief Read resampled audio data from the buffer.
   *
   * @param data Pointer to the buffer to fill
   * @param len Number of bytes to read
   * @return size_t Number of bytes actually read
   */
  size_t readBytes(uint8_t* data, size_t len) override {
    return resampling_buffer.readArray(data, len);
  }

  /**
   * @brief Recalculate the resampling step size based on buffer fill level.
   *
   * @return float The new step size
   */
  float recalculate() { return resampling_buffer.recalculate(); }

  /**
   * @brief Set the allowed resampling range as a percent.
   *
   * @param rangePercent Allowed range in percent (e.g., 5.0 for ± 5%)
   */
  void setStepRangePercent(float rangePercent) {
    resampling_buffer.setStepRangePercent(rangePercent);
  }

  /**
   * @brief Defines the fill level (percent) that must be reached once,
   * right after begin(), before any data is returned by readBytes().
   * Defaults to 50%; 0 disables priming. See
   * AdaptiveResamplingBuffer::setStartFillPercent().
   *
   * @param percent Fill level in percent (e.g. 50.0 for 50%)
   */
  void setStartFillPercent(float percent) {
    resampling_buffer.setStartFillPercent(percent);
  }

  /// True once the buffer has reached setStartFillPercent() (or always
  /// true if priming is disabled).
  bool isPrimed() { return resampling_buffer.isPrimed(); }

  /**
   * @brief Get the current actual buffer fill level in percent.
   *
   * @return float Current fill level (0-100)
   */
  float levelPercentActual() { return resampling_buffer.levelPercent(); }

  /**
   * @brief Get the fill level at the last calculation in percent.
   *
   * @return float Last calculated fill level (0-100)
   */
  float levelPercent() { return resampling_buffer.levelPercentSmoothed(); }

  /**
   * @brief Set the Kalman filter parameters.
   *
   * @param process_noise Process noise covariance (Q)
   * @param measurement_noise Measurement noise covariance (R)
   */
  void setKalmanParameters(float process_noise, float measurement_noise) {
    resampling_buffer.setKalmanParameters(process_noise, measurement_noise);
  }

  /**
   * @brief Set the PID controller parameters.
   *
   * @param p_value Proportional gain
   * @param i_value Integral gain
   * @param d_value Derivative gain
   */
  void setPIDParameters(float p_value, float i_value, float d_value) {
    resampling_buffer.setPIDParameters(p_value, i_value, d_value);
  }

 protected:
  AdaptiveResamplingBuffer resampling_buffer;
};

}  // namespace audio_tools
