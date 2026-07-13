#pragma once

#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {
/**
 * @class LMSEchoCancellationStream
 * @brief Echo cancellation with adaptive LMS filtering for microcontrollers.
 *
 * This class implements echo cancellation using an adaptive FIR filter (LMS
 * algorithm). It estimates the echo path and subtracts the estimated echo from
 * the microphone input. write() buffers the playback (speaker) signal into a
 * sliding delay line; readBytes() pulls mic samples from the wrapped Stream
 * and filters each one against that delay line. cancel(rec, play, out, len)
 * is a Stream-free alternative that feeds both signals explicitly in
 * lock-step (useful for testing or non-duplex pipelines) and is verified to
 * produce output identical to the write()/readBytes() path.
 *
 * Strengths:
 * - Tiny and simple: O(filter_len) work per sample, no FFT, trivial memory
 *   footprint -- a reasonable fit for small microcontrollers with a short,
 *   simple echo path (e.g. a small speaker directly coupled to a nearby mic).
 * - The delay line is a real sliding window (fixed-size ring buffer that
 *   evicts as it fills), so it keeps adapting indefinitely rather than
 *   stalling after some fixed number of samples.
 *
 * Weaknesses:
 * - Plain LMS with a fixed step size (mu), not NLMS -- the right mu depends
 *   on signal amplitude and echo strength and needs per-deployment tuning;
 *   too large diverges, too small converges too slowly to track a changing
 *   echo path.
 * - No double-talk protection: if near-end speech happens while the speaker
 *   is playing, plain LMS can't tell that apart from "the echo path
 *   changed" and may adapt in the wrong direction (contrast with
 *   MDFEchoCancellationStream's two-path foreground/background scheme).
 * - Only models a short, simple echo path well: filter_len must cover the
 *   actual echo delay in samples, and cost grows linearly with it -- not
 *   the right tool for a long, reverberant echo tail.
 * - No sample-rate/timing compensation: write() (speaker) and readBytes()
 *   (mic) must be driven at the same pace for the delay line to stay
 *   time-aligned; there is no internal resampling or drift correction for
 *   a mismatched full-duplex pipeline.
 */
template <typename T = int16_t>
 class LMSEchoCancellationStream : public AudioStream {
 public:
  /**
   * @brief Constructor
   * @param in Reference to the input stream (microphone or audio input)
   * @param lag_samples Number of samples to delay the echo subtraction
   * (default: 0)
   * @param buffer_size Size of the internal ring buffer (default: 512)
   */
  LMSEchoCancellationStream(Stream& in, size_t lag_samples = 0, size_t buffer_size = 512,
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
   * @brief Store the output signal (sent to speaker). The samples are pushed
   * into the internal delay line so that readBytes() can later use them as
   * the echo reference.
   * @param buf Pointer to PCM data sent to the speaker (T*)
   * @param len Number of bytes in buf
   * @return Number of bytes processed
   */
  size_t write(const uint8_t* buf, size_t len) override {
    const T* data = (const T*)buf;
    size_t samples = len / sizeof(T);
    for (size_t i = 0; i < samples; ++i) pushPlayback(data[i]);
    return samples * sizeof(T);
  }

  /**
   * @brief Read input and remove echo, using the playback signal previously
   * buffered via write() as the reference for the adaptive filter.
   * @param buf Pointer to buffer to store processed input (T*)
   * @param len Number of bytes to read
   * @return Number of bytes read from input
   */
  size_t readBytes(uint8_t* buf, size_t len) override {
    size_t read = p_io->readBytes(buf, len);
    size_t actual_samples = read / sizeof(T);
    T* data = (T*)buf;

    for (size_t i = 0; i < actual_samples; ++i) {
      data[i] = filterSample(data[i]);
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
    ref_vec.resize(filter_len, 0);
  }

  /**
   * @brief Reset the internal buffer and lag state.
   */
  void reset() {
    ring_buffer.reset();
    ring_buffer.resize(buffer_size + lag);
    for (size_t j = 0; j < lag; j++) {
      ring_buffer.write(0);
    }
    filter.assign(filter_len, 0.0f);
    ref_vec.resize(filter_len, 0);
  }

  /**
   * @brief Process echo cancellation on arrays of samples. Self-contained:
   * feeds the playback reference and the recorded signal in lock-step, so it
   * can be used directly without going through the Stream write()/readBytes()
   * pair.
   * @param rec Pointer to received (microphone) signal samples
   * @param play Pointer to playback (speaker) signal samples
   * @param out Pointer to output buffer for echo-cancelled signal
   * @param len Number of samples to process
   */
  void cancel(const T* rec, const T* play, T* out, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      pushPlayback(play[i]);
      out[i] = filterSample(rec[i]);
    }
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
  Vector<T> ref_vec;

  /**
   * @brief Push a playback (speaker) sample into the delay line, evicting the
   * oldest sample once the window exceeds filter_len + lag so that the
   * reference always reflects the most recent playback history.
   */
  void pushPlayback(T sample) {
    ring_buffer.write(sample);
    T dummy;
    while ((size_t)ring_buffer.available() > filter_len + lag) {
      ring_buffer.read(dummy);
    }
  }

  /**
   * @brief Estimate and subtract the echo for a single microphone sample,
   * using the oldest filter_len samples currently buffered (i.e. the
   * playback signal delayed by lag samples) as the adaptive filter's
   * reference, then update the filter taps (NLMS-free, plain LMS).
   */
  T filterSample(T mic_sample) {
    if ((size_t)ring_buffer.available() < filter_len + lag) {
      // Not enough playback history buffered yet: pass through unmodified.
      return mic_sample;
    }

    ring_buffer.peekArray(ref_vec.data(), filter_len);

    float echo_est = 0.0f;
    for (size_t k = 0; k < filter_len; ++k) {
      echo_est += filter[k] * (float)ref_vec[k];
    }

    float mic = (float)mic_sample;
    float error = mic - echo_est;

    // LMS update
    for (size_t k = 0; k < filter_len; ++k) {
      filter[k] += adaptation_rate * error * (float)ref_vec[k];
    }

    return (T)error;
  }
};

}  // namespace audio_tools