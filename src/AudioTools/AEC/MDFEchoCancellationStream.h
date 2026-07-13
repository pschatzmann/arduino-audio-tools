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
 * using the high-performance MDF algorithm.
 *
 * @tparam Allocator Custom allocator for memory management (default:
 * std::allocator)
 */
template <typename Allocator = std::allocator<uint8_t>>
class MDFEchoCancellationStream : public AudioStream {
 public:
  /**
   * @brief Constructor (single channel)
   * @param in Reference to the input stream (microphone or audio input)
   * @param filterLength Length of echo cancellation filter in samples
   * @param fftDriver FFT driver instance from AudioFFT
   * @param alloc Allocator instance (optional)
   */
  MDFEchoCancellationStream(Stream& in, int filterLength,
                            AudioFFTBase& fftDriver,
                            const Allocator& alloc = Allocator())
      : p_io(&in),
        p_fft(&fftDriver),
        canceller(getFrameSize(), filterLength, fftDriver, alloc) {}

  /**
   * @brief Constructor (multiple channels)
   * @param in Reference to the input stream
   * @param filterLength Length of echo cancellation filter in samples
   * @param nbMic Number of microphone channels
   * @param nbSpeakers Number of speaker channels
   * @param fftDriver FFT driver instance from AudioFFT
   * @param alloc Allocator instance (optional)
   */
  MDFEchoCancellationStream(Stream& in, int filterLength,
                            AudioFFTBase& fftDriver,
                            const Allocator& alloc = Allocator())
      : p_io(&in),
        p_fft(&fftDriver),
        canceller(getFrameSize(), filterLength, fftDriver,
                  alloc) {}

  /**
   * @brief Store the output signal (sent to speaker)
   * @param buf Pointer to PCM data sent to the speaker
   * @param len Number of bytes in buf
   * @return Number of bytes processed
   */
  size_t write(const uint8_t* buf, size_t len) override {
    return canceller.write(buf, len);
  }

  /**
   * @brief Read input and remove echo (using buffered playback)
   * @param buf Pointer to buffer to store processed input
   * @param len Number of bytes to read
   * @return Number of bytes read from input
   */
  size_t readBytes(uint8_t* buf, size_t len) override {
    size_t read = p_io->readBytes(buf, len);
    size_t samples = read / sizeof(echo_int16_t);
    int frame_size = getFrameSize();

    if (samples >= (size_t)frame_size) {
      canceller.capture((const echo_int16_t*)buf, (echo_int16_t*)buf);
      return frame_size * sizeof(echo_int16_t);
    }
    return read;
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
  void setFilterLen(int len) { canceller.setFilterLen(len); }

  /**
   * @brief Get filter length
   * @return Current filter length
   */
  int getFilterLen() { return canceller.getFilterLen(); }

  /**
   * @brief Reset echo canceller state
   */
  void reset() { canceller.reset(); }

  /**
   * @brief Get underlying MDF echo canceller instance
   * @return Reference to MDFEchoCancellation object
   */
  MDFEchoCancellation<Allocator>& getEchoCanceller() { return canceller; }

 protected:
  Stream* p_io = nullptr;
  AudioFFTBase* p_fft = nullptr;
  MDFEchoCancellation<Allocator> canceller;

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
