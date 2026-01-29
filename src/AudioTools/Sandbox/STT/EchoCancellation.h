#pragma once

#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {
/**
 * @class EchoCancellation
 * @brief Echo cancellation with adaptive LMS filtering for microcontrollers.
 *
 * This class implements echo cancellation using an adaptive FIR filter (LMS
 * algorithm). It estimates the echo path and subtracts the estimated echo from
 * the microphone input.
 */
template <typename T = int16_t>
 class EchoCancellation : public AudioStream {
 public:
  /**
   * @brief Constructor
   * @param in Reference to the input stream (microphone or audio input)
   * @param lag_samples Number of samples to delay the echo subtraction
   * (default: 0)
   * @param buffer_size Size of the internal ring buffer (default: 512)
   */
  EchoCancellation(Stream& in, size_t lag_samples = 0, size_t buffer_size = 512,
                   size_t filter_len = 32, float mu = 0.001f)
      : lag(lag_samples),
        buffer_size(buffer_size),
        filter_len(filter_len),
        adaptation_rate(mu) {
    p_io = &in;
    filter.resize(filter_len, 0.0f);
    reset();
  }

  /**
   * @brief Store the output signal (sent to speaker)
   * @param buf Pointer to PCM data sent to the speaker (T*)
   * @param len Number of bytes in buf
   * @return Number of bytes processed
   */
  size_t write(const uint8_t* buf, size_t len) override {
    // Store output signal in queue for echo estimation
    return ring_buffer.writeArray((T*)buf, len / sizeof(T)) *
           sizeof(T);
  }

  /**
   * @brief Read input and remove echo (subtract output signal with lag)
   * @param buf Pointer to buffer to store processed input (T*)
   * @param len Number of bytes to read
   * @return Number of bytes read from input
   */
  size_t readBytes(uint8_t* buf, size_t len) override {
    size_t read = p_io->readBytes(buf, len);
    size_t actual_samples = read / sizeof(T);
    T* data = (T*)buf;
    Vector<T> ref_vec(filter_len, 0);
    ring_buffer.peekArray(ref_vec.data(), filter_len);
    for (size_t i = 0; i < actual_samples; ++i) {
      // Build the reference vector for the adaptive filter
      float echo_est = 0.0f;
      for (size_t k = 0; k < filter_len; ++k) {
        echo_est += filter[k] * ref_vec[k];
      }
      float mic = (float)data[i];
      float error = mic - echo_est;
      data[i] = (T)error;
      // LMS update
      for (size_t k = 0; k < filter_len; ++k) {
        filter[k] += adaptation_rate * error * ref_vec[k];
      }
      T dummy;
      ring_buffer.read(dummy);  // Advance the queue
      // Shift ref_vec left and append dummy
      for (size_t k = 0; k < filter_len - 1; ++k) {
        ref_vec[k] = ref_vec[k + 1];
      }
      ref_vec[filter_len - 1] = dummy;
    }
    return read;
  }

  /**
   * @brief Set the lag (delay) in samples for echo cancellation.
   * @param lag_samples Number of samples to delay the echo subtraction
   */
  void setLag(size_t lag_samples) { lag = lag_samples; }

  /**
   * @brief Set the adaptation rate (mu) for the LMS algorithm.
   * @param mu Adaptation rate
   */
  void setMu(float mu) { adaptation_rate = mu; }

  /**
   * @brief Set the filter length for the adaptive filter.
   * @param len Length of the adaptive filter
   */
  void setFilterLen(size_t len) {
    filter_len = len;
    filter.resize(filter_len, 0.0f);
  }

  /**
   * @brief Reset the internal buffer and lag state.
   */
  void reset() {
    ring_buffer.resize(buffer_size + lag);
    for (size_t j = 0; j < lag; j++) {
      ring_buffer.write(0);
    }
    filter.assign(filter_len, 0.0f);
  }

 protected:
  Stream* p_io = nullptr;
  RingBuffer<T> ring_buffer{0};
  size_t buffer_size;
  size_t lag;  // lag in samples
  // Adaptive filter
  size_t filter_len;
  float adaptation_rate = 0.01f;
  Vector<float> filter;
};

}  // namespace audio_tools