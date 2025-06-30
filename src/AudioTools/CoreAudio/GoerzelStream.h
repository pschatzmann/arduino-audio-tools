#pragma once
#include <math.h>

#include "AudioStreams.h"

/**
 * @defgroup dsp DSP
 * @ingroup main
 * @brief Digital Signal Processing
 **/

namespace audio_tools {

/**
 * @brief Configuration for Goertzel algorithm detectors
 * @ingroup dsp
 * @author pschatzmann
 * @copyright GPLv3
 */
struct GoertzelConfig : public AudioInfo {
  /// Target frequency to detect in Hz (same for all channels)
  float target_frequency = 440.0f;
  /// Number of samples to process per block (N)
  int block_size = 205;
  /// Detection threshold for magnitude (normalized samples, typically 0.1
  /// to 1.0)
  float threshold = 0.5f;

  GoertzelConfig() = default;
  /// Copy constructor from AudioInfo
  GoertzelConfig(const AudioInfo& info) : AudioInfo(info) {
    target_frequency = 440.0f;
    block_size = 205;
    threshold = 0.5f;
  }
};

/**
 * @brief Single-frequency Goertzel algorithm implementation for tone detection.
 * The Goertzel algorithm is an efficient way to compute the magnitude of a
 * specific frequency component in a signal. It's particularly useful for
 * DTMF (touch-tone) detection and other single-frequency detection
 * applications.
 *
 * This implementation expects normalized float samples in the range
 * [-1.0, 1.0]. For different sample formats, use GoertzelStream which handles
 * format conversion.
 *
 * @ingroup dsp
 * @author pschatzmann
 * @copyright GPLv3
 */
class GoertzelDetector {
 public:
  GoertzelDetector() = default;

  /**
   * @brief Initialize the Goertzel detector with configuration
   * @param config GoertzelConfig containing all parameters
   */
  void begin(const GoertzelConfig& config) {
    this->config = config;
    this->sample_count = 0;

    // Calculate the coefficient k and omega
    float omega = (2.0f * M_PI * config.target_frequency) / config.sample_rate;
    coeff = 2.0f * cos(omega);

    // Reset state variables
    reset();
  }

  /**
   * @brief Process a single sample
   * @param sample Input sample (normalized float in range [-1.0, 1.0])
   * @return True if block is complete and magnitude is available
   */
  bool processSample(float sample) {
    // Goertzel algorithm core computation
    float s0 = sample + coeff * s1 - s2;
    s2 = s1;
    s1 = s0;

    sample_count++;

    // Check if we've processed a complete block
    if (sample_count >= config.block_size) {
      // Calculate magnitude squared
      float real = s1 - s2 * cos(2.0f * M_PI * config.target_frequency /
                                 config.sample_rate);
      float imag =
          s2 * sin(2.0f * M_PI * config.target_frequency / config.sample_rate);
      magnitude_squared = real * real + imag * imag;
      magnitude = sqrt(magnitude_squared);

      // Reset for next block
      reset();
      return true;
    }

    return false;
  }

  /**
   * @brief Get the magnitude of the detected frequency
   * @return Magnitude value
   */
  float getMagnitude() const { return magnitude; }

  /**
   * @brief Get the squared magnitude (more efficient if you don't need the
   * actual magnitude)
   * @return Squared magnitude value
   */
  float getMagnitudeSquared() const { return magnitude_squared; }

  /**
   * @brief Check if the detected magnitude is above a threshold
   * @param threshold Threshold value
   * @return True if magnitude is above threshold
   */
  bool isDetected(float threshold) const { return magnitude > threshold; }

  /**
   * @brief Reset the detector state
   */
  void reset() {
    s1 = 0.0f;
    s2 = 0.0f;
    sample_count = 0;
    magnitude = 0.0f;
    magnitude_squared = 0.0f;
  }

  /**
   * @brief Get the target frequency
   */
  float getTargetFrequency() const { return config.target_frequency; }

  /**
   * @brief Get the block size
   */
  int getBlockSize() const { return config.block_size; }

  /**
   * @brief Get the current configuration
   */
  const GoertzelConfig& getConfig() const { return config; }

 private:
  GoertzelConfig config;
  float coeff = 0.0f;

  // State variables
  float s1 = 0.0f;
  float s2 = 0.0f;
  int sample_count = 0;

  // Results
  float magnitude = 0.0f;
  float magnitude_squared = 0.0f;
};

/**
 * @brief AudioStream implementation that processes audio data through Goertzel
 * algorithm. This class acts as a passthrough filter that can detect specific
 * frequencies in the audio stream.
 *
 * Supports multiple channels with independent frequency detection per channel.
 * The number of detectors is determined by the channels field in
 * GoertzelConfig.
 *
 * Supports multiple sample formats:
 * - 8-bit: unsigned samples (0-255), converted to signed (-128 to 127)
 * - 16-bit: signed samples (-32768 to 32767)
 * - 24-bit: signed samples stored as 3 bytes, little-endian
 * - 32-bit: signed samples (-2147483648 to 2147483647)
 *
 * @ingroup dsp
 * @author pschatzmann
 * @copyright GPLv3
 */
class GoertzelStream : public AudioStream {
 public:
  GoertzelStream() = default;

  void setAudioInfo(AudioInfo info) override {
    AudioStream::setAudioInfo(info);
    single_config.copyFrom(info);
    // Initialize detectors for each channel
    detectors.resize(info.channels);
  }

  /**
   * @brief Initialize with GoertzelConfig
   * @param config Configuration object containing all parameters
   */
  bool begin(const GoertzelConfig& config) {
    this->single_config = config;
    // Set up audio info from config
    setAudioInfo(config);
    return begin();
  }

  bool begin() {
    for (int i = 0; i < info.channels; i++) {
      detectors[i].begin(single_config);
    }
    return info.channels > 0;
  }

  /// Defines/Changes the input & output
  void setStream(Stream& in) {
    p_stream = &in;
    p_print = &in;
  }

  /// Defines/Changes the output target
  void setOutput(Print& out) { p_print = &out; }

  /**
   * @brief Set detection callback function for channel-aware frequency
   * detection
   * @param callback Function to call when frequency is detected, includes
   * channel info
   */
  void setChannelDetectionCallback(void (*callback)(
      int channel, float frequency, float magnitude, void* ref)) {
    channel_detection_callback = callback;
  }

  /**
   * @brief Process audio data and pass it through
   * @param data Input audio data
   * @param len Length of data
   * @return Number of bytes written to output
   */
  size_t write(const uint8_t* data, size_t len) override {
    if (p_print == nullptr) return 0;

    // Process samples for detection
    processSamples(data, len);

    /// If there is no output stream, we just return the length
    if (p_print == nullptr) return len;

    // Pass data through unchanged
    return p_print->write(data, len);
  }

  /**
   * @brief Read data from input stream and process it
   * @param data Output buffer
   * @param len Length to read
   * @return Number of bytes read
   */
  size_t readBytes(uint8_t* data, size_t len) override {
    if (p_stream == nullptr) return 0;

    size_t result = p_stream->readBytes(data, len);

    // Process samples for detection
    processSamples(data, result);

    return result;
  }

  /**
   * @brief Get the current magnitude for frequency detection
   * @param channel Channel index (default: 0)
   */
  float getCurrentMagnitude(int channel = 0) {
    if (channel >= 0 && channel < detectors.size()) {
      return detectors[channel].getMagnitude();
    }
    return 0.0f;
  }

  /**
   * @brief Check if frequency is currently detected
   * @param channel Channel index (default: 0)
   */
  bool isFrequencyDetected(int channel = 0) {
    if (channel >= 0 && channel < detectors.size()) {
      return detectors[channel].isDetected(single_config.threshold);
    }
    return false;
  }

  /**
   * @brief Get the current configuration
   */
  const GoertzelConfig& getConfig() const { return single_config; }

  /// Defines a reference to any object that should be available in the callback
  void setReference(void* ref) { this->ref = ref; }

 protected:
  Vector<GoertzelDetector> detectors;
  // Configuration objects
  GoertzelConfig single_config;
  // Stream pointers
  Stream* p_stream = nullptr;
  Print* p_print = nullptr;

  // Callback functions
  void (*channel_detection_callback)(int channel, float frequency,
                                     float magnitude, void* ref) = nullptr;
  void* ref = this;

  /**
   * @brief Helper method to check detection and call callback
   * @param channel Channel index
   */
  void checkDetection(int channel) {
    if (channel >= 0 && channel < detectors.size()) {
      float magnitude = detectors[channel].getMagnitude();
      if (magnitude > single_config.threshold) {
        float frequency = detectors[channel].getTargetFrequency();
        if (channel_detection_callback) {
          channel_detection_callback(channel, frequency, magnitude, ref);
        }
      }
    }
  }

  /**
   * @brief Template helper to process samples of a specific type
   */
  template <typename T>
  void processSamplesOfType(const uint8_t* data, size_t data_len,
                            int channels) {
    const T* samples = reinterpret_cast<const T*>(data);
    size_t num_samples = data_len / sizeof(T);

    for (size_t i = 0; i < num_samples; i++) {
      float normalized = NumberConverter::toFloatT<T>(samples[i]);
      int channel = i % channels;
      if (detectors[channel].processSample(normalized)) {
        checkDetection(channel);
      }
    }
  }

  /**
   * @brief Generic sample processing method for both write and readBytes
   */
  void processSamples(const uint8_t* data, size_t data_len) {
    int channels = single_config.channels;

    switch (single_config.bits_per_sample) {
      case 8:
        processSamplesOfType<uint8_t>(data, data_len, channels);
        break;
      case 16:
        processSamplesOfType<int16_t>(data, data_len, channels);
        break;
      case 24:
        processSamplesOfType<int24_t>(data, data_len, channels);
        break;
      case 32:
        processSamplesOfType<int32_t>(data, data_len, channels);
        break;
      default:
        LOGE("Unsupported bits_per_sample: %d", single_config.bits_per_sample);
        break;
    }
  }
};

}  // namespace audio_tools