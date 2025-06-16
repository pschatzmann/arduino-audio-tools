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
   * @brief Initialize internal buffers based on audio info.
   * @return true if initialization succeeded.
   */
  bool begin() {
    buffer.resize(buffer_size * info.channels * info.bits_per_sample / 8);
    freq.resize(info.channels);
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
    for (int ch = 0; ch < info.channels; ch++) {
      freq[ch] = detectFrequencyForChannel(ch, samples, len);
      if (freq_callback) freq_callback(ch, freq[ch]);
    }
  }

  /**
   * @brief Performs autocorrelation to estimate frequency for a single channel.
   * @tparam T Sample type.
   * @param ch Channel index.
   * @param samples Pointer to audio samples.
   * @param len Number of samples.
   * @return Detected frequency in Hz.
   */
  template <class T>
  float detectFrequencyForChannel(int ch, T* samples, size_t len) {
    LOGD("detectFrequencyForChannel: %d / len: %u", ch, (unsigned int)len);
    // Prepare variables for autocorrelation
    int sample_rate = info.sample_rate;
    int channels = info.channels;
    int buffer_size = len / info.channels;

    // Autocorrelation lag range: 1000 Hz max, 50 Hz min
    size_t min_lag = sample_rate / 1000;
    size_t max_lag = sample_rate / 50;
    if (max_lag >= buffer_size) max_lag = buffer_size - 1;

    LOGD("lag min/max: %u / %u", (unsigned)min_lag, (unsigned)max_lag);

    float max_corr = 0.0f;
    size_t best_lag = 0;
    for (size_t lag = min_lag; lag < max_lag; ++lag) {
      float sum = 0.0f;
      for (size_t i = 0; i < buffer_size - lag; ++i) {
        sum += samples[i * channels] * samples[(i + lag) * channels];
      }
      if (sum > max_corr) {
        max_corr = sum;
        best_lag = lag;
      }
    }

    LOGD("best_lag: %u / max_corr: %f", (unsigned)best_lag, max_corr);

    if (best_lag == 0) return 0.0f;
    return (float)sample_rate / best_lag;
  }
};

/**
 * @brief Detects frequency using upward zero crossings in audio samples.
 * 
 * This class estimates the frequency by counting the number of samples
 * between upward zero crossings (negative to positive transitions).
 * It supports multiple channels and different sample formats.
 * 
 * Usage:
 *  - Feed audio data via write() or readBytes().
 *  - Call frequency(channel) to get the detected frequency for a channel.
 *  - Optionally, set a callback to be notified when a new frequency is detected.
 */
class FrequencyDetectorZeroCrossing : public AudioStream {
 public:
  /**
   * @brief Default constructor.
   */
  FrequencyDetectorZeroCrossing() = default;

  /**
   * @brief Construct with output stream.
   * @param out Output stream for writing audio data.
   */
  FrequencyDetectorZeroCrossing(Print& out) { p_out = &out; };

  /**
   * @brief Construct with input stream.
   * @param in Input stream for reading audio data.
   */
  FrequencyDetectorZeroCrossing(Stream& in) {
    p_out = &in;
    p_in = &in;
  };

  /**
   * @brief Initialize with audio configuration.
   * @param info AudioInfo structure describing the audio format.
   * @return true if initialization succeeded.
   */
  bool begin(AudioInfo info) {
    setAudioInfo(info);
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
    switch (info.bits_per_sample) {
      case 16:
        detect<int16_t>((int16_t*)data, len / sizeof(int16_t));
        break;
      case 24:
        detect<int24_t>((int24_t*)data, len / sizeof(int24_t));
        break;
      case 32:
        detect<int32_t>((int32_t*)data, len / sizeof(int32_t));
        break;
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
    switch (info.bits_per_sample) {
      case 16:
        detect<int16_t>((int16_t*)data, len / sizeof(int16_t));
        break;
      case 24:
        detect<int24_t>((int24_t*)data, len / sizeof(int24_t));
        break;
      case 32:
        detect<int32_t>((int32_t*)data, len / sizeof(int32_t));
        break;
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
    return states[channel].freq;
  }

  /**
   * @brief Sets a callback function to be called when a new frequency is detected.
   * @param callback Function pointer: void callback(int channel, float freq)
   */
  void setFrequencyCallback(void (*callback)(int channel, float freq)) {
    freq_callback = callback;
  }

 protected:
  /**
   * @brief Holds state for each channel during zero crossing detection.
   */
  struct ChannelState {
    int count = 0;        ///< Sample count since last zero crossing
    bool active = false;  ///< True if counting is active
    float freq = 0.0f;    ///< Last detected frequency
  };
  Vector<ChannelState> states;       ///< State for each channel
  Print* p_out = nullptr;            ///< Output stream pointer
  Stream* p_in = nullptr;            ///< Input stream pointer
  int count = 0;                     ///< Sample count (unused, kept for compatibility)
  bool active = false;               ///< Counting active flag (unused, kept for compatibility)
  void (*freq_callback)(int channel, float freq); ///< Frequency callback function

  /**
   * @brief Detects frequency for all channels using zero crossing method.
   * @tparam T Sample type (int16_t, int24_t, int32_t)
   * @param samples Pointer to audio samples.
   * @param len Number of samples.
   */
  template <class T>
  void detect(T* samples, size_t len) {
    states.resize(info.channels);
    for (int ch = 0; ch < info.channels; ch++) {
      detectChannel(ch, samples, len);
    }
  }

  /**
   * @brief Detects frequency for a single channel by counting upward zero crossings.
   * @tparam T Sample type.
   * @param channel Channel index.
   * @param samples Pointer to audio samples.
   * @param len Number of samples.
   */
  template <class T>
  void detectChannel(int channel, T* samples, size_t len) {
    ChannelState& state = states[channel];
    for (int i = channel; i < (int)(len - info.channels); i += info.channels) {
      if (state.active) state.count++;
      // Detect upward zero crossing (negative to positive)
      if (samples[i] <= 0 && samples[i + info.channels] > 0) {
        if (state.count > 0) {
          state.freq = (1.0f * info.sample_rate) / state.count;
          if (freq_callback) freq_callback(channel, state.freq);
        }
        state.count = 0;
        state.active = true;
      }
    }
  }
};

}  // namespace audio_tools
