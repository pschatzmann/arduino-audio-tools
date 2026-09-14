#pragma once

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioStreams.h"

namespace audio_tools {

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
