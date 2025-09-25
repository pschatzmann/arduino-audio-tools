#pragma once

#include <math.h>

#include "AudioStreams.h"

#ifndef M_PI
#define M_PI (3.14159265358979323846f)
#endif

/**
 * @defgroup dsp DSP
 * @ingroup main
 * @brief Digital Signal Processing
 **/

namespace audio_tools {

/**
 * @brief Configuration for Goertzel algorithm detectors
 *
 * This structure extends AudioInfo to include Goertzel-specific parameters.
 * It defines the frequency detection behavior, audio format, and processing
 * parameters for the Goertzel algorithm implementation.
 *
 * @ingroup dsp
 * @author pschatzmann
 * @copyright GPLv3
 */
struct GoertzelConfig : public AudioInfo {
  /// Target frequency to detect in Hz (same for all channels)
  float target_frequency = 0.0f;
  /// Number of samples to process per block (N) - affects detection latency and
  /// accuracy
  int block_size = 205;
  /// Detection threshold for magnitude (normalized samples, typically 0.1
  /// to 1.0)
  float threshold = 0.5f;
  /// Volume factor for normalization - scales input samples before processing
  float volume = 1.0f;
  /// channel used for detection when used in a stream
  uint8_t channel = 0;

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
  bool begin(const GoertzelConfig& config) {
    this->config = config;
    this->sample_count = 0;

    if (config.target_frequency == 0.0f) {
      return false;
    }

    // Calculate the coefficient k and omega
    float omega = (2.0f * M_PI * config.target_frequency) / config.sample_rate;
    coeff = 2.0f * cos(omega);

    // Reset state variables
    reset();
    return true;
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
      magnitude_squared = (real * real) + (imag * imag);
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
   * @brief Check if the detected magnitude is above the configured threshold
   * @return True if magnitude is above configured threshold
   */
  bool isDetected() const { return isDetected(config.threshold); }

  /**
   * @brief Reset the detector state
   */
  void reset() {
    s1 = 0.0f;
    s2 = 0.0f;
    sample_count = 0;
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

  void setReference(void* ref) { this->reference = ref; }

  void* getReference() { return reference; }

 protected:
  GoertzelConfig config;
  float coeff = 0.0f;
  void* reference = nullptr;

  // State variables
  float s1 = 0.0f;
  float s2 = 0.0f;
  int sample_count = 0;

  // Results
  float magnitude = 0.0f;
  float magnitude_squared = 0.0f;
};

/**
 * @brief AudioStream-based multi-frequency Goertzel detector for real-time
 * audio analysis.
 *
 * GoertzelStream enables efficient detection of one or more target frequencies
 * in a continuous audio stream. It acts as a transparent filter: audio data
 * flows through unchanged, while the class analyzes the signal for specified
 * tones.
 *
 * Key Features:
 * - Detects multiple frequencies simultaneously (DTMF, tone detection, etc.)
 * - Supports runtime addition of frequencies via addFrequency()
 * - Works with various sample formats: 8/16/24/32-bit (see below)
 * - Channel-aware: can analyze a specific channel in multi-channel audio
 * - Callback system: notifies user when a frequency is detected above threshold
 * - Non-intrusive: does not modify or block audio data
 * - Configurable detection parameters (block size, threshold, volume, etc.)
 *
 * Usage:
 * 1. Configure the stream with GoertzelConfig or AudioInfo (sample rate,
 * channels, etc.)
 * 2. Add one or more target frequencies using addFrequency()
 * 3. Optionally set a detection callback with setFrequencyDetectionCallback()
 * 4. Use write() or readBytes() to process audio data; detection runs
 * automatically
 *
 * Supported sample formats:
 * - 8-bit: unsigned (0-255), internally converted to signed (-128 to 127)
 * - 16-bit: signed (-32768 to 32767)
 * - 24-bit: signed, packed format
 * - 32-bit: signed (-2^31 to 2^31-1)
 *
 *
 * @ingroup dsp
 * @author pschatzmann
 * @copyright GPLv3
 */
class GoertzelStream : public AudioStream {
 public:
  // Default Constructor with no output or input
  GoertzelStream() = default;
  GoertzelStream(Print& out) { setOutput(out); }
  GoertzelStream(AudioOutput& out) { setOutput(out); }
  GoertzelStream(Stream& io) { setStream(io); };
  GoertzelStream(AudioStream& io) { setStream(io); };

  /**
   * @brief Set audio format and initialize detector array
   *
   * This method is called when the audio format changes. It updates the
   * internal configuration and resizes the detector array to match the
   * number of audio channels.
   *
   * @param info Audio format information (sample rate, channels, bits per
   * sample)
   */
  void setAudioInfo(AudioInfo info) override {
    AudioStream::setAudioInfo(info);
    default_config.copyFrom(info);
    // Initialize detectors for each frequency
    begin();
  }

  /**
   * @brief Returns a default GoertzelConfig instance with standard parameters
   *
   * This utility method provides a convenient way to obtain a default
   * configuration for the Goertzel algorithm. The returned config can be
   * customized before use.
   *
   * @return GoertzelConfig with default values
   */
  GoertzelConfig defaultConfig() {
    GoertzelConfig result;
    return result;
  }

  /**
   * @brief Initialize with GoertzelConfig
   *
   * Sets up the stream with specific Goertzel algorithm parameters.
   * This method configures the audio format and detection parameters
   * in one call.
   *
   * @param config Configuration object containing all parameters
   * @return true if initialization successful, false otherwise
   */
  bool begin(GoertzelConfig config) {
    config.target_frequency = 0;
    if (config.channel >= config.channels) {
      LOGE("GoertzelStream: channel %d out of range (max %d)", config.channel,
           config.channels);
      return false;
    }
    this->default_config = config;

    // Set up audio info from config
    setAudioInfo(config);
    return begin();
  }

  /**
   * @brief Initialize detectors for all frequencies
   *
   * Creates and configures individual Goertzel detectors for frequency. Each
   * detector will independently for the target frequency.
   *
   * @return true if at least one channel is configured, false otherwise
   */
  bool begin() {
    int i = 0;
    detectors.clear();
    for (float freq : frequencies) {
      GoertzelConfig cfg = default_config;
      cfg.target_frequency = freq;
      GoertzelDetector detector;
      if (i < references.size()) {
        detector.setReference(references[i]);
      }
      detector.begin(cfg);
      detectors.push_back(detector);
      i++;
    }
    sample_no = 0;
    return true;
  }

  /**
   * @brief Stop the Goertzel detectors and clear resources
   */
  void end() {
    detectors.clear();
    AudioStream::end();
  }

  /// Defines/Changes the input & output
  void setStream(Stream& in) {
    p_stream = &in;
    p_print = &in;
  }

  /// Defines/Changes the input & output
  void setStream(AudioStream& io) {
    setStream((Stream&)io);
    addNotifyAudioChange(io);
  }

  /// Defines/Changes the output target
  void setOutput(Print& out) { p_print = &out; }

  /// Defines/Changes the output target
  void setOutput(AudioOutput& out) {
    setOutput((Print&)out);
    addNotifyAudioChange(out);
  }

  /**
   * @brief Set detection callback function for channel-aware frequency
   * detection
   *
   * Registers a callback function that will be called when the target frequency
   * is detected on any channel. The callback receives the channel number,
   * detected frequency, magnitude, and a user reference pointer.
   *
   * @param callback Function to call when frequency is detected, includes
   * channel info
   */
  void setFrequencyDetectionCallback(void (*callback)(float frequency,
                                                      float magnitude,
                                                      void* ref)) {
    frequency_detection_callback = callback;
  }

  /**
   * @brief Process audio data and pass it through
   *
   * This method receives audio data, processes it through the Goertzel
   * detectors for frequency analysis, and then passes the unmodified
   * data to the output stream. This allows for real-time frequency
   * detection without affecting the audio flow.
   *
   * @param data Input audio data buffer
   * @param len Length of data in bytes
   * @return Number of bytes written to output stream
   */
  size_t write(const uint8_t* data, size_t len) override {
    // Process samples for detection
    processSamples(data, len);

    /// If there is no output stream, we just return the length
    if (p_print == nullptr) return len;

    // Pass data through unchanged
    return p_print->write(data, len);
  }

  /**
   * @brief Read data from input stream and process it
   *
   * Reads audio data from the input stream, processes it through the
   * Goertzel detectors for frequency analysis, and returns the data
   * to the caller. This allows for frequency detection in pull-mode
   * audio processing.
   *
   * @param data Output buffer to store read data
   * @param len Maximum number of bytes to read
   * @return Number of bytes actually read
   */
  size_t readBytes(uint8_t* data, size_t len) override {
    if (p_stream == nullptr) return 0;

    size_t result = p_stream->readBytes(data, len);

    // Process samples for detection
    processSamples(data, result);

    return result;
  }

  /**
   * @brief Get the current configuration
   *
   * Returns a reference to the current Goertzel configuration, including
   * audio format, detection parameters, and processing settings.
   *
   * @return Reference to the current GoertzelConfig
   */
  const GoertzelConfig& getConfig() const { return default_config; }

  /**
   * @brief Set reference pointer for callback context
   *
   * Defines a reference to any object that should be available in the
   * detection callback. This allows the callback to access application
   * context or other objects.
   *
   * @param ref Pointer to user-defined context object
   */
  void setReference(void* ref) { this->ref = ref; }

  /**
   * @brief Get detector for specific channel
   *
   * Provides access to the GoertzelDetector instance for a given channel.
   * This allows for direct inspection of detector state and results.
   *
   * @param no Channel index (0-based)
   * @return Reference to the GoertzelDetector for the specified channel
   */
  GoertzelDetector& getDetector(int no) { return detectors[no]; }

  /**
   * @brief Add a frequency to the detection list
   *
   * @param freq Frequency in Hz to add to the detection list
   */
  void addFrequency(float freq) { frequencies.push_back(freq); }

  /**
   * @brief Add a frequency to the detection list with a custom reference
   * pointer
   *
   * This method allows you to associate a user-defined reference (context
   * pointer) with a specific frequency. The reference will be passed to the
   * detection callback when this frequency is detected, enabling per-frequency
   * context handling.
   *
   * @param freq Frequency in Hz to add to the detection list
   * @param ref Pointer to user-defined context object for this frequency
   */
  void addFrequency(float freq, void* ref) {
    frequencies.push_back(freq);
    references.push_back(ref);
  }

 protected:
  // Core detection components
  Vector<GoertzelDetector>
      detectors;                  ///< One detector per frequency in frequencies
  Vector<float> frequencies;      ///< List of frequencies to detect
  Vector<void*> references;       ///< List of frequencies to detect
  GoertzelConfig default_config;  ///< Current algorithm configuration
  // Stream I/O components
  Stream* p_stream = nullptr;  ///< Input stream for reading audio data
  Print* p_print = nullptr;    ///< Output stream for writing audio data

  // Callback system
  void (*frequency_detection_callback)(float frequency, float magnitude,
                                       void* ref) =
      nullptr;       ///< User callback for detection events
  void* ref = this;  ///< User-defined reference for callback context

  size_t sample_no = 0;  ///< Sample counter for channel selection

  /**
   * @brief Check if a detector has detected its frequency and invoke callback
   */
  void checkDetection(GoertzelDetector& detector) {
    float magnitude = detector.getMagnitude();
    if (magnitude > 0.0f)
      LOGD("frequency: %f / magnitude: %f / threshold: %f", detector.getTargetFrequency(), magnitude, default_config.threshold);

    if (magnitude > default_config.threshold) {
      float frequency = detector.getTargetFrequency();
      void* reference = detector.getReference();
      if (reference == nullptr) reference = ref;
      if (frequency_detection_callback != nullptr) {
        frequency_detection_callback(frequency, magnitude, reference);
      }
    }
  }

  /**
   * @brief Template helper to process samples of a specific type
   *
   * Converts audio samples from their native format to normalized floats,
   * applies volume scaling, and feeds them to the appropriate channel
   * detectors. This method handles the format conversion and channel
   * distribution automatically.
   *
   * @tparam T Sample data type (uint8_t, int16_t, int24_t, int32_t)
   * @param data Raw audio data buffer
   * @param data_len Length of data buffer in bytes
   * @param channels Number of audio channels (for sample distribution)
   */
  template <typename T = int16_t>
  void processSamplesOfType(const uint8_t* data, size_t data_len,
                            int channels) {
    const T* samples = reinterpret_cast<const T*>(data);
    size_t num_samples = data_len / sizeof(T);

    for (size_t i = 0; i < num_samples; i++) {
      // select only samples for the configured channel
      if (sample_no % channels == default_config.channel) {
        float normalized = clip(NumberConverter::toFloatT<T>(samples[i]) *
                                default_config.volume);
        LOGD("sample: %f", normalized);                        
        // process all frequencies
        for (auto& detector : detectors) {
          if (detector.processSample(normalized)) {
            checkDetection(detector);
          }
        }
      }
      sample_no++;
    }
  }

  /**
   * @brief Clip audio values to prevent overflow
   *
   * Ensures that normalized audio samples stay within the valid range
   * of [-1.0, 1.0] to prevent algorithm instability.
   *
   * @param value Input audio sample value
   * @return Clipped value in range [-1.0, 1.0]
   */
  float clip(float value) {
    // Clip the value to the range [-1.0, 1.0]
    if (value > 1.0f) return 1.0f;
    if (value < -1.0f) return -1.0f;
    return value;
  }

  /**
   * @brief Generic sample processing method for both write and readBytes
   *
   * This method serves as the central dispatcher for audio sample processing.
   * It examines the configured sample format and routes the data to the
   * appropriate type-specific processing method. This approach eliminates
   * code duplication between write() and readBytes() methods.
   *
   * Supported formats:
   * - 8-bit: Unsigned samples (0-255)
   * - 16-bit: Signed samples (-32768 to 32767)
   * - 24-bit: Signed samples in 3-byte packed format
   * - 32-bit: Signed samples (-2^31 to 2^31-1)
   *
   * @param data Raw audio data buffer
   * @param data_len Length of data buffer in bytes
   */
  void processSamples(const uint8_t* data, size_t data_len) {
    // return if there is nothing to detect
    if (detectors.empty()) return;
    int channels = default_config.channels;

    switch (default_config.bits_per_sample) {
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
        LOGE("Unsupported bits_per_sample: %d", default_config.bits_per_sample);
        break;
    }
  }
};

}  // namespace audio_tools