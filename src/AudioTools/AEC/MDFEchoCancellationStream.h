#pragma once

#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "MDFEchoCancellation.h"

namespace audio_tools {

/**
 * @class MDFEchoCancellationStream
 * @brief MDF (Multi-Delay block Frequency) echo cancellation with stream API.
 *
 * This class wraps MDFEchoCancellation and provides a stream-based interface
 * for easy integration with AudioTools streaming pipelines. It processes audio
 * using a block, frequency-domain NLMS-style filter (a header-only port of
 * speexdsp's MDF algorithm) with a two-path (foreground/background) filter
 * switch for double-talk robustness. write()/readBytes() must be called with
 * buffers that are exact multiples of the frame size
 * (AudioFFTBase::config().length); any trailing partial frame is dropped,
 * not buffered for next time. Constructing (really: first using) this class
 * re-begin()s the FFT driver you pass in at a *different* size (2x the
 * configured frame size) -- dedicate that AudioFFTBase instance to the
 * canceller; don't share it with other FFT work or re-begin() it afterwards.
 *
 * Strengths:
 * - A meaningfully stronger algorithm than LMSEchoCancellationStream: block-
 *   frequency processing handles much longer echo tails without the
 *   linear-in-taps cost of a time-domain filter, and the two-path filter
 *   gives real (if simply-tuned) protection against double-talk.
 * - Multi-channel support (nbMic x nbSpeakers) is built into the algorithm
 *   via the second constructor.
 * - Verified to actually converge: on a synthetic attenuated-echo signal
 *   (no near-end noise), residual echo energy drops to roughly 1e-7 of the
 *   original within a few hundred frames (see test_mdf_converges in
 *   tests-cmake/stt/stt_test.cpp).
 *
 * Weaknesses / things to know before relying on this:
 * - Only verified with the AudioRealFFT driver. Other AudioFFTBase drivers
 *   (AudioKissFFT, AudioCmsisFFT, AudioESP32FFT, AudioEspressifFFT,
 *   AudioFixedFFT) have their own, separate bin-access implementations
 *   that have NOT been audited the way AudioRealFFT was -- using one of
 *   them here is unverified territory.
 * - This file (and MDFEchoCancellation.h) was, by a wide margin, the most
 *   bug-dense code in this AEC sandbox: it did not compile at all, and once
 *   it did, several more bugs (word types hardcoded to int16_t/int32_t
 *   despite float-only arithmetic, a 0/0 = NaN in the leak estimate, a
 *   leftover fixed-point (int32_t) cast that truncated the entire adaptive
 *   filter gradient to zero, plus two bugs in the shared AudioRealFFT.h FFT
 *   driver) kept it from ever attenuating any echo. All of these are now
 *   fixed and covered by tests, but treat further changes to this file with
 *   real caution -- see src/AudioTools/AEC/README.md for the full list.
 * - The two-path switching thresholds and adaptation heuristics are the
 *   original speexdsp defaults, unmodified, and only exercised here against
 *   a single synthetic tone with no near-end noise. Real acoustic echo
 *   (multi-tap reverberant paths, concurrent near-end speech, non-
 *   stationary noise) is not yet validated.
 *
 * Uses the audio_tools::Allocator abstraction (see
 * AudioTools/CoreAudio/AudioBasic/Collections/Allocator.h) for the
 * echo-state buffers, defaulting to the shared DefaultAllocator -- pass a
 * different Allocator (e.g. AllocatorPSRAM on ESP32) to place them
 * elsewhere.
 */
class MDFEchoCancellationStream : public AudioStream {
 public:
  /**
   * @brief Constructor (single channel)
   * @param in Reference to the input stream (microphone or audio input)
   * @param filterLength Length of echo cancellation filter in samples
   * @param fftDriver FFT driver instance from AudioFFT
   * @param alloc Allocator used for the echo-state buffers (optional,
   *              defaults to the shared DefaultAllocator)
   */
  MDFEchoCancellationStream(Stream& in, int filterLength,
                            AudioFFTBase& fftDriver,
                            Allocator& alloc = DefaultAllocator)
      : p_io(&in),
        p_fft(&fftDriver),
        canceller(filterLength, fftDriver, alloc) {}

  /**
   * @brief Constructor (multiple channels)
   * @param in Reference to the input stream
   * @param filterLength Length of echo cancellation filter in samples
   * @param nbMic Number of microphone channels
   * @param nbSpeakers Number of speaker channels
   * @param fftDriver FFT driver instance from AudioFFT
   * @param alloc Allocator used for the echo-state buffers (optional,
   *              defaults to the shared DefaultAllocator)
   */
  MDFEchoCancellationStream(Stream& in, int filterLength, int nbMic,
                            int nbSpeakers, AudioFFTBase& fftDriver,
                            Allocator& alloc = DefaultAllocator)
      : p_io(&in),
        p_fft(&fftDriver),
        canceller(filterLength, nbMic, nbSpeakers, fftDriver, alloc) {}

  /**
   * @brief Store the output signal (sent to speaker). Buffers are processed
   * in frame-sized chunks (see getFrameSize()); any trailing partial frame
   * is not consumed.
   * @param buf Pointer to PCM data sent to the speaker
   * @param len Number of bytes in buf
   * @return Number of bytes actually processed (a multiple of the frame size)
   */
  size_t write(const uint8_t* buf, size_t len) override {
    int frame_size = getFrameSize();
    size_t samples = len / sizeof(echo_int16_t);
    size_t frames = samples / (size_t)frame_size;
    const echo_int16_t* data = (const echo_int16_t*)buf;
    for (size_t f = 0; f < frames; f++) {
      canceller.playback(data + f * frame_size);
    }
    return frames * (size_t)frame_size * sizeof(echo_int16_t);
  }

  /**
   * @brief Read input and remove echo (using buffered playback). Buffers are
   * processed in frame-sized chunks (see getFrameSize()); any trailing
   * partial frame is left unprocessed and not reported as read.
   * @param buf Pointer to buffer to store processed input
   * @param len Number of bytes to read
   * @return Number of bytes actually processed (a multiple of the frame size)
   */
  size_t readBytes(uint8_t* buf, size_t len) override {
    size_t read = p_io->readBytes(buf, len);
    size_t samples = read / sizeof(echo_int16_t);
    int frame_size = getFrameSize();
    size_t frames = samples / (size_t)frame_size;
    echo_int16_t* data = (echo_int16_t*)buf;
    for (size_t f = 0; f < frames; f++) {
      canceller.capture(data + f * frame_size, data + f * frame_size);
    }
    return frames * (size_t)frame_size * sizeof(echo_int16_t);
  }

  void setAudioInfo(AudioInfo info) override {
    AudioStream::setAudioInfo(info);
    canceller.setSamplingRate(info.sample_rate);
    canceller.setMicChannels(info.channels);
    canceller.setSpeakerChannels(info.channels);
  }

  /**
   * @brief Set filter length (number of filter blocks)
   * @param len Length of the adaptive filter in blocks
   */
  void setFilterLen(int len) { canceller.setFilterLength(len); }

  /**
   * @brief Get filter length
   * @return Current filter length
   */
  int getFilterLen() { return canceller.getFilterLength(); }

  /**
   * @brief Reset echo canceller state
   */
  void reset() { canceller.reset(); }

  /**
   * @brief Get underlying MDF echo canceller instance
   * @return Reference to MDFEchoCancellation object
   */
  MDFEchoCancellation& getEchoCanceller() { return canceller; }

 protected:
  Stream* p_io = nullptr;
  AudioFFTBase* p_fft = nullptr;
  MDFEchoCancellation canceller;

  /**
   * @brief Get frame size from FFT configuration
   * @return Frame size in samples
   */
  int getFrameSize() {
    if (p_fft) {
      return p_fft->config().length;
    }
    return 512;  // default fallback
  }
};

}  // namespace audio_tools
