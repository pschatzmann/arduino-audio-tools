#pragma once

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Detects frequency using autocorrelation on audio samples.
 *
 * This class analyzes audio data to estimate the dominant frequency
 * by applying the autocorrelation method. It supports multiple audio
 * channels and different sample formats (16, 24, 32 bits).
 *
 * Usage:
 *  - Feed audio data via write() or readBytes().
 *  - Call frequency(channel) to get the detected frequency for a channel.
 *  - Optionally, set a callback to be notified when a new frequency is detected.
 *
 * Based on: https://github.com/akellyirl/AutoCorr_Freq_detect
 */
class FrequencyDetectorAutoCorrelation : public AudioStream {
 public:
  /**
   * @brief Default constructor. The buffer size is auto-derived from the
   * AudioInfo passed to begin() unless setBufferSize() is called first.
   */
  FrequencyDetectorAutoCorrelation() = default;

  /**
   * @brief Construct with buffer size.
   * @param bufferSize Number of samples to buffer for analysis.
   */
  FrequencyDetectorAutoCorrelation(int bufferSize) {
    buffer_size = bufferSize;
  };

  /**
   * @brief Construct with buffer size and output stream.
   * @param bufferSize Number of samples to buffer for analysis.
   * @param out Output stream for writing audio data.
   */
  FrequencyDetectorAutoCorrelation(int bufferSize, Print& out) {
    p_out = &out;
    buffer_size = bufferSize;
  };

  /**
   * @brief Construct with buffer size and input stream.
   * @param bufferSize Number of samples to buffer for analysis.
   * @param in Input stream for reading audio data.
   */
  FrequencyDetectorAutoCorrelation(int bufferSize, Stream& in) {
    p_out = &in;
    p_in = &in;
    buffer_size = bufferSize;
  }

  /**
   * @brief Initialize with audio configuration.
   * @param info AudioInfo structure describing the audio format.
   * @return true if initialization succeeded.
   */
  bool begin(AudioInfo info) {
    setAudioInfo(info);
    return begin();
  }

  /**
   * @brief Sets the analysis buffer size explicitly (in samples). If not
   * called, begin() derives a size that covers the lowest detectable
   * frequency (50Hz) for the configured sample rate.
   */
  void setBufferSize(int bufferSize) { buffer_size = bufferSize; }

  /**
   * @brief Initialize internal buffers based on audio info.
   * @return true if initialization succeeded.
   */
  bool begin() {
    if (buffer_size <= 0) {
      // must be large enough to contain at least one period of the lowest
      // frequency we try to detect (50Hz, see detectFrequencyForChannel)
      buffer_size = info.sample_rate / 50 + 16;
    }
    buffer.resize(buffer_size * info.channels * info.bits_per_sample / 8);
    freq.resize(info.channels);
    conf.resize(info.channels);
    return AudioStream::begin();
  }

  /**
   * @brief Returns the number of bytes available for reading.
   */
  int available() override {
    if (p_in) return p_in->available();
    return 0;
  }

  /**
   * @brief Returns the number of bytes available for writing.
   */
  int availableForWrite() override {
    if (p_out) return p_out->availableForWrite();
    return DEFAULT_BUFFER_SIZE;
  }

  /**
   * @brief Reads bytes from the input stream and processes them for frequency detection.
   * @param data Buffer to store read bytes.
   * @param len Number of bytes to read.
   * @return Number of bytes actually read.
   */
  size_t readBytes(uint8_t* data, size_t len) override {
    size_t result = p_in->readBytes(data, len);
    for (int i = 0; i < len; i++) {
      buffer.write(data[i]);
      if (buffer.isFull()) {
        // Process buffer when full, based on sample format
        switch (info.bits_per_sample) {
          case 16:
            detect<int16_t>((int16_t*)buffer.data(),
                            buffer.available() / sizeof(int16_t));
            break;
          case 24:
            detect<int24_t>((int24_t*)buffer.data(),
                            buffer.available() / sizeof(int24_t));
            break;
          case 32:
            detect<int32_t>((int32_t*)buffer.data(),
                            buffer.available() / sizeof(int32_t));
            break;
        }
        buffer.reset();
      }
    }
    return result;
  }

  /**
   * @brief Writes bytes to the output stream and processes them for frequency detection.
   * @param data Buffer containing audio data.
   * @param len Number of bytes to write.
   * @return Number of bytes actually written.
   */
  virtual size_t write(const uint8_t* data, size_t len) override {
    for (int i = 0; i < len; i++) {
      buffer.write(data[i]);
      if (buffer.isFull()) {
        // Process buffer when full, based on sample format
        switch (info.bits_per_sample) {
          case 16:
            detect<int16_t>((int16_t*)buffer.data(),
                            buffer.available() / sizeof(int16_t));
            break;
          case 24:
            detect<int24_t>((int24_t*)buffer.data(),
                            buffer.available() / sizeof(int24_t));
            break;
          case 32:
            detect<int32_t>((int32_t*)buffer.data(),
                            buffer.available() / sizeof(int32_t));
            break;
        }
        buffer.reset();
      }
    }

    size_t result = len;
    if (p_out != nullptr) result = p_out->write(data, len);
    return result;
  }

  /**
   * @brief Returns the last detected frequency for the given channel.
   * @param channel Channel index.
   * @return Detected frequency in Hz, or 0 if invalid channel.
   */
  float frequency(int channel) {
    if (channel >= info.channels) {
      LOGE("Invalid channel: %d", channel);
      return 0;
    }
    return freq[channel];
  }

  /**
   * @brief Returns a tonality/periodicity confidence for the given channel,
   * in the range 0.0 (no periodic structure - noise-like) to 1.0 (strongly
   * periodic - tone/voiced/music-like). This is the normalized autocorrelation
   * peak (peak correlation at the detected lag, divided by the signal energy
   * at lag 0) and can be used to distinguish genuine tonal audio from
   * broadband noise that merely happens to be loud.
   * @param channel Channel index.
   * @return Confidence in [0.0, 1.0], or 0 for an invalid channel.
   */
  float confidence(int channel) {
    if (channel >= info.channels) {
      LOGE("Invalid channel: %d", channel);
      return 0;
    }
    return conf[channel];
  }

  /**
   * @brief Convenience check: is the given channel currently tonal (periodic)
   * rather than noise-like, based on confidence().
   * @param channel Channel index.
   * @param threshold Minimum confidence to be considered tonal (default 0.3).
   */
  bool isTonal(int channel, float threshold = 0.3f) {
    return confidence(channel) >= threshold;
  }

  /**
   * @brief Returns a default AudioInfo configuration.
   */
  AudioInfo defaultConfig() {
    AudioInfo result;
    return result;
  }

  /**
   * @brief Sets a callback function to be called when a new frequency is detected.
   * @param callback Function pointer: void callback(int channel, float freq)
   */
  void setFrequencyCallback(void (*callback)(int channel, float freq)) {
    freq_callback = callback;
  }

 protected:
  Vector<float> freq;                ///< Stores detected frequency for each channel
  Vector<float> conf;                ///< Stores tonality confidence (0-1) for each channel
  Print* p_out = nullptr;            ///< Output stream pointer
  Stream* p_in = nullptr;            ///< Input stream pointer
  void (*freq_callback)(int channel, float freq); ///< Frequency callback function
  int buffer_size = 0;               ///< Buffer size in samples
  SingleBuffer<uint8_t> buffer;      ///< Buffer for incoming audio data

  /**
   * @brief Detects frequency for all channels using autocorrelation.
   * @tparam T Sample type (int16_t, int24_t, int32_t)
   * @param samples Pointer to audio samples.
   * @param len Number of samples.
   */
  template <class T>
  void detect(T* samples, size_t len) {
    freq.resize(info.channels);
    conf.resize(info.channels);
    for (int ch = 0; ch < info.channels; ch++) {
      float channel_confidence = 0.0f;
      freq[ch] = detectFrequencyForChannel(ch, samples, len, channel_confidence);
      conf[ch] = channel_confidence;
      if (freq_callback) freq_callback(ch, freq[ch]);
    }
  }

  /**
   * @brief Performs autocorrelation to estimate frequency for a single channel.
   * @tparam T Sample type.
   * @param ch Channel index.
   * @param samples Pointer to audio samples.
   * @param len Number of samples.
   * @param confidenceOut Set to the normalized autocorrelation peak (0.0-1.0):
   * how periodic/tonal the block is, independent of its loudness. Values near
   * 0 indicate noise-like (non-periodic) content; values near 1 indicate a
   * strong tone/voiced/harmonic signal.
   * @return Detected frequency in Hz.
   */
  template <class T>
  float detectFrequencyForChannel(int ch, T* samples, size_t len,
                                   float& confidenceOut) {
    LOGD("detectFrequencyForChannel: %d / len: %u", ch, (unsigned int)len);
    confidenceOut = 0.0f;
    // Prepare variables for autocorrelation
    int sample_rate = info.sample_rate;
    int channels = info.channels;
    int buffer_size = len / info.channels;

    // Autocorrelation lag range: 1000 Hz max, 50 Hz min
    size_t min_lag = sample_rate / 1000;
    size_t max_lag = sample_rate / 50;
    if (max_lag >= buffer_size) max_lag = buffer_size - 1;

    LOGD("lag min/max: %u / %u", (unsigned)min_lag, (unsigned)max_lag);

    // Energy at lag 0 - used to normalize the correlation peak into a
    // loudness-independent confidence score
    double energy = 0.0;
    for (int i = 0; i < buffer_size; ++i) {
      double s = (double)samples[i * channels];
      energy += s * s;
    }

    double max_corr = 0.0;
    size_t best_lag = 0;
    for (size_t lag = min_lag; lag < max_lag; ++lag) {
      double sum = 0.0;
      for (size_t i = 0; i < buffer_size - lag; ++i) {
        sum += (double)samples[i * channels] * (double)samples[(i + lag) * channels];
      }
      if (sum > max_corr) {
        max_corr = sum;
        best_lag = lag;
      }
    }

    LOGD("best_lag: %u / max_corr: %f", (unsigned)best_lag, max_corr);

    if (energy > 0.0) {
      confidenceOut = (float)(max_corr / energy);
      if (confidenceOut > 1.0f) confidenceOut = 1.0f;
      if (confidenceOut < 0.0f) confidenceOut = 0.0f;
    }

    if (best_lag == 0) return 0.0f;
    return (float)sample_rate / best_lag;
  }
};

}  // namespace audio_tools
