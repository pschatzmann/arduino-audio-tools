/**
 * @file CodecDSF.h
 * @brief DSF (DSD Stream File) format decoder implementation
 * @author pschatzmann
 * @copyright GPLv3
 *
 * This file contains the implementation of a DSF decoder that converts Direct
 * Stream Digital (DSD) audio data to Pulse Code Modulation (PCM) format. The
 * decoder supports the DSF file format which is commonly used for
 * high-resolution audio distribution.
 *
 * Key features:
 * - DSF file header parsing and validation
 * - DSD bitstream to PCM conversion with configurable decimation
 * - BiQuad low-pass filtering for anti-aliasing
 * - Streaming-compatible operation for real-time processing
 * - Support for stereo DSD files (DSD64 and higher sample rates)
 *
 */

#pragma once
// #pragma GCC optimize("Ofast")
#pragma GCC optimize("O3")

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/AudioFilter/Filter.h"
#include "AudioTools/CoreAudio/Buffers.h"

/**
 * @defgroup dsd DSD Audio
 * @ingroup codecs
 * @brief Direct Stream Digital (DSD) audio format support
 */

/// Buffer size for DSD data processing - must accommodate decimation step
#define DSD_BUFFER_SIZE 1024 * 2

namespace audio_tools {

/**
 * @brief Metadata structure for DSF (DSD Stream File) format
 * @ingroup dsd
 * @author pschatzmann
 *
 * Contains format information and metadata extracted from DSF file headers,
 * including DSD sample rates, data sizes, and calculated PCM conversion
 * parameters.
 */
struct DSFMetadata : public AudioInfo {
  DSFMetadata() = default;
  DSFMetadata(int rate) { sample_rate = rate; }
  uint32_t dsd_sample_rate =
      0;                        ///< DSD sample rate (e.g. 2822400 Hz for DSD64)
  uint64_t dsd_data_bytes = 0;  ///< Size of DSD bitstream data in bytes
  uint8_t dsd_bits = 1;         ///< BitSize always 1!
  uint64_t pcm_frames = 0;  ///< Estimated number of PCM frames after conversion
  float duration_sec = 0;   ///< Approximate audio duration in seconds
  uint32_t dsd_buffer_size =
      DSD_BUFFER_SIZE;  ///< Internal buffer size for DSD processing
  float filter_q = 0.5f; //1.41f;
  float filter_cutoff = 0.4f;  ///< Cutoff frequency as fraction of Nyquist
  int output_buffer_size = 1024;
};

/**
 * @brief Header structures for DSF (DSD Stream File) format
 * @ingroup dsd
 *
 * These packed structures define the binary layout of DSF file headers,
 * allowing direct parsing of the file format without manual byte manipulation.
 */

/// DSF file prefix containing file identification and basic information
struct __attribute__((packed)) DSDPrefix {
  char id[4];               // "DSD "
  uint64_t chunkSize;       // 28
  uint64_t fileSize;        // total file size
  uint64_t metadataOffset;  // offset to "ID3 " chunk (0 if none)
};

/// DSF format chunk containing audio format parameters
struct __attribute__((packed)) DSFFormat {
  char id[4];                    // "fmt "
  uint64_t chunkSize;            // 52
  uint32_t formatVersion;        // 1
  uint32_t formatID;             // 0
  uint32_t channelType;          // e.g., 2 for stereo
  uint32_t channelNum;           // number of channels
  uint32_t samplingFrequency;    // e.g., 2822400
  uint32_t bitsPerSample;        // 1
  uint64_t sampleCount;          // total samples per channel
  uint32_t blockSizePerChannel;  // e.g., 4096
  uint32_t reserved;             // 0
};

/// DSF data chunk header containing audio data size information
struct __attribute__((packed)) DSFDataHeader {
  char id[4];          // "data"
  uint64_t chunkSize;  // size of DSD data
  // followed by: uint8_t rawData[chunkSize];
};

/**
 * @brief DSF (DSD Stream File) format decoder
 * @ingroup dsd
 * @author pschatzmann
 *
 * Decodes DSF files containing Direct Stream Digital (DSD) audio data and
 * converts it to PCM format. DSF is a file format that stores DSD audio
 * streams, commonly used for high-resolution audio. This decoder:
 *
 * - Parses DSF file headers to extract format information
 * - Buffers incoming DSD bitstream data
 * - Applies decimation and low-pass filtering for anti-aliasing
 * - Outputs converted PCM audio samples
 *
 * The decoder uses BiQuad low-pass filters for high-quality anti-aliasing
 * during the DSD to PCM conversion process, replacing traditional FIR filter
 * implementations for better performance and modularity.
 *
 * @note Supports mono and stereo DSD files with sample rates >= 2.8224 MHz
 * (DSD64)
 *
 */
class DSFDecoder : public AudioDecoder {
 public:
  DSFDecoder() = default;
  DSFDecoder(DSFMetadata metaData) { setMetaData(metaData); };

  AudioInfo audioInfo() override { return meta; }

  /// Can be used to set up alternative sample rate (default is 44100 Hz) and
  /// bits
  void setAudioInfo(AudioInfo from) override {
    TRACED();
    AudioDecoder::setAudioInfo(from);
    meta.copyFrom(from);
    if (isHeaderAvailable()){
      // Ensure PCM buffer is allocated based on the new audio info
      int buffer_size = getOutputBufferSize();
      pcmBuffer.resize(buffer_size);
      channelAccum.resize(meta.channels);
      channelIntegrator.resize(meta.channels);

      setupTargetPCMRate();
      setupDecimationStep();
    }
  }

  /**
   * @brief Initialize the decoder
   * @return true if initialization successful
   *
   * Sets up the decoder state, initializes buffers, and configures the low-pass
   * filters with default parameters. The filters are initialized with a cutoff
   * frequency of 40% of the Nyquist frequency to provide effective
   * anti-aliasing.
   */
  bool begin() {
    TRACED();
    dsdBuffer.resize(meta.dsd_buffer_size);
    dsdBuffer.reset();
    headerParsed = false;
    headerSize = 0;
    dataSize = 0;
    filePos = 0;
    decimationStep = 64;
    max_value = 0;

    // update decimaten step & filter parameters
    isActive = true;

    return true;
  }

  void end() override { isActive = false; }

  /**
   * @brief Get DSF file metadata
   * @return Reference to DSFMetadata structure containing format information
   *
   * Returns metadata extracted from the DSF file header, including DSD sample
   * rate, data size, estimated PCM frames, and calculated duration.
   */
  const DSFMetadata getMetadata() { return meta; }

  void setMetaData(DSFMetadata metaData) {
    meta = metaData;
    AudioDecoder::setAudioInfo(meta);
  }

  /**
   * @brief Check if decoder is ready
   * @return true if DSF header has been successfully parsed
   *
   * Indicates whether the decoder has successfully parsed the DSF file header
   * and is ready to process audio data.
   */
  bool isHeaderAvailable() { return headerParsed; }

  operator bool() { return isActive; }

  /**
   * @brief Main entry point for processing incoming DSF data
   * @param data Incoming DSF file data bytes
   * @param len Number of bytes in data buffer
   * @return Number of bytes consumed (always returns len for streaming
   * compatibility)
   *
   * Processes incoming DSF file data in two phases:
   * 1. Header parsing: Extracts format information from DSF file header
   * 2. Audio processing: Buffers DSD data and converts to PCM output
   *
   * The method is designed for streaming operation and always reports full
   * consumption of input data for compatibility with streaming frameworks.
   */
  size_t write(const uint8_t* data, size_t len) {
    LOGD("write: %u", (unsigned)len);
    size_t i = 0;

    // Phase 1: Parse DSF header to extract format information
    i += processHeader(data, len, i);

    // Phase 2: Process audio data (buffer DSD + convert to PCM)
    if (headerParsed && i < len) {
      i += processDSDData(data, len, i);
    }

    return len;  // Always report full consumption for streaming compatibility
  }

 protected:
  // Header parsing state
  size_t headerSize;          ///< Current size of accumulated header data
  bool headerParsed = false;  ///< Flag indicating if header parsing is complete
  bool isActive = false;  ///< Flag indicating if decoder is active and ready
  uint64_t dataSize;      ///< Size of audio data section in bytes
  size_t filePos;         ///< Current position in DSF file

  // Processing buffers and state
  SingleBuffer<uint8_t> pcmBuffer{0};  ///< Buffer for PCM output samples -
                                       ///< supports multi-channel up to 32-bit
  Vector<float> channelAccum;  ///< Accumulator for each channel during DSD to
                               ///< PCM conversion
  Vector<LowPassFilter<float>>
      channelFilters;                ///< Anti-aliasing filters for each channel
  RingBuffer<uint8_t> dsdBuffer{0};  ///< Ring buffer for DSD data
  uint32_t decimationStep;  ///< Decimation factor for DSD to PCM conversion
  Vector<float> channelIntegrator;  ///< Integrator state for each channel (for
                                    ///< better DSD conversion)

  // Metadata
  DSFMetadata meta;  ///< Extracted DSF file metadata
  float max_value = 0.0f;

  /// The buffer size is defined in the metadata: it must be at least 1 frame
  int getOutputBufferSize() {
    int frame_size = meta.bits_per_sample / 8 * meta.channels;
    if (meta.bits_per_sample == 24) frame_size = 4 * meta.channels;
    int buffer_size = frame_size;
    if (meta.output_buffer_size > buffer_size)
      buffer_size = meta.output_buffer_size;
    return buffer_size;
  }

  /**
   * @brief Process header data until header is complete or data is exhausted
   * @param data Input data buffer
   * @param len Length of input data
   * @param startPos Starting position in input buffer
   * @return Number of bytes processed for header parsing
   *
   * Accumulates header bytes and attempts to parse the DSF file header.
   * When a complete and valid header is found, sets headerParsed flag and
   * updates decimation parameters.
   */
  size_t processHeader(const uint8_t* data, size_t len, size_t startPos) {
    if (headerParsed) return 0;
    LOGI("processHeader: %u (%u)", (unsigned)len, (unsigned)startPos);

    // Check for DSD header magic
    if (memcmp(data, "DSD ", 4) != 0) {
      LOGE("Invalid DSF header magic");
      return 0;
    }

    int dataPos = findTag("data", data, len);
    int fmtPos = findTag("fmt ", data, len);
    if (dataPos < 0 || fmtPos < 0) {
      LOGE("DSF header not found in data (fmt: %d, data: %d)", fmtPos, dataPos);
      return 0;  // No valid header found
    }
    // parse the data
    parseFMT(data + fmtPos, len - fmtPos);
    parseData(data + dataPos, len - dataPos);
    headerParsed = true;

    // update audio info and initialize filters
    setAudioInfo(meta);

    return dataPos + sizeof(DSFDataHeader);
  }

  /**
   * @brief Process DSD audio data: buffer it and convert to PCM when possible
   * @param data Input data buffer containing DSD audio data
   * @param len Length of input data
   * @param startPos Starting position in input buffer
   * @return Number of bytes processed for audio data
   *
   * Buffers incoming DSD data and triggers PCM conversion when sufficient
   * data is available for processing.
   */
  size_t processDSDData(const uint8_t* data, size_t len, size_t startPos) {
    LOGD("processDSDData: %u (%u)", (unsigned)len, (unsigned)startPos);
    size_t bytesProcessed = 0;

    // Buffer as much DSD data as possible
    bytesProcessed += bufferDSDData(data, len, startPos);

    // Convert buffered DSD data to PCM output
    convertDSDToPCM();

    return bytesProcessed;
  }

  /**
   * @brief Buffer incoming DSD data into ring buffer
   * @param data Input data buffer
   * @param len Length of input data
   * @param startPos Starting position in input buffer
   * @return Number of bytes successfully buffered
   *
   * Copies DSD data bytes into the internal ring buffer until either all
   * data is consumed or the buffer becomes full.
   */
  size_t bufferDSDData(const uint8_t* data, size_t len, size_t startPos) {
    int write_len = len - startPos;
    if (write_len > dsdBuffer.availableForWrite()) {
      write_len = dsdBuffer.availableForWrite();
    }
    dsdBuffer.writeArray(data + startPos, write_len);
    filePos += write_len;

    return write_len;
  }

  /**
   * @brief Convert buffered DSD data to PCM samples and output them
   *
   * Performs the core DSD to PCM conversion process using integrator-based
   * approach:
   * 1. Integrates DSD bits over the decimation period for each channel
   * 2. Converts DSD bits to analog values (-1 or +1) with proper delta-sigma
   * handling
   * 3. Applies low-pass filtering to remove high-frequency noise
   * 4. Converts filtered values to PCM samples
   * 5. Outputs PCM samples for all channels
   *
   * The conversion uses BiQuad low-pass filters for anti-aliasing, providing
   * better audio quality than simple decimation.
   *
   * DSF format uses byte interleaving: each byte contains 8 DSD samples for one
   * channel, and channels are interleaved at the byte level (not bit level).
   */
  void convertDSDToPCM() {
    while (hasEnoughData()) {
      // Initialize accumulators
      for (int ch = 0; ch < meta.channels; ch++) {
        channelAccum[ch] = 0.0f;
      }
      // Initialize integrator states
      for (int ch = 0; ch < meta.channels; ch++) {
        channelIntegrator[ch] = 0.0f;
      }

      // Accumulate DSD samples over decimation period
      // DSF uses byte interleaving: bytes alternate between channels
      int bytesPerDecimationStep = decimationStep / 8;
      int samplesProcessed = 0;

      for (int i = 0; i < bytesPerDecimationStep && !dsdBuffer.isEmpty(); i++) {
        for (int ch = 0; ch < meta.channels && !dsdBuffer.isEmpty(); ch++) {
          uint8_t dsdByte;
          if (dsdBuffer.read(dsdByte)) {
            // Each byte contains 8 DSD samples for the current channel
            // Use integrator-based approach for better DSD conversion
            for (int bit = 0; bit < 8; bit++) {
              int channelBit = (dsdByte >> (7 - bit)) & 1;  // MSB first in DSF

              // Delta-sigma integration: accumulate the difference
              channelIntegrator[ch] += channelBit ? 1.0f : -1.0f;

              // Apply decay to prevent DC buildup
              channelIntegrator[ch] *= 0.9999f;
            }

            // Add integrated value to channel accumulator
            channelAccum[ch] += channelIntegrator[ch];
            samplesProcessed += 8;
          }
        }
      }

      float samplesPerChannel = samplesProcessed / meta.channels;

      if (samplesPerChannel > 0) {
        for (int ch = 0; ch < meta.channels; ch++) {
          // Normalize by sample count and apply scaling factor
          channelAccum[ch] = channelAccum[ch] / samplesPerChannel * 0.8f;
          if (meta.filter_cutoff > 0.0f &&
              meta.filter_q > 0.0f) {  // Only apply filter if configured
            // Apply low-pass filter to remove high-frequency noise
            channelAccum[ch] = channelFilters[ch].process(channelAccum[ch]);
          }
          //Serial.print(channelAccum[ch]);
          //Serial.print(" ");

          // Convert to PCM sample and store in buffer
          writePCMSample(clip(channelAccum[ch]));
        }
      }

      //Serial.println();

      // Output the PCM samples for all channels
      if (pcmBuffer.isFull()) {
        size_t frameSize = pcmBuffer.available();
        size_t written =
            getOutput()->write((uint8_t*)pcmBuffer.data(), frameSize);
        if (written != frameSize) {
          LOGE(
              "Failed to write PCM samples: expected %zu bytes, wrote %zu "
              "bytes",
              frameSize, written);
        }
        pcmBuffer.reset();
      }
    }
  }

  /**
   * @brief Clips audio values to valid range
   * @param value Input audio value
   * @return Clipped value in range [-1.0, 1.0]
   *
   * Ensures that filtered audio values stay within the valid range to
   * prevent clipping artifacts in the final PCM output.
   */
  float clip(float value) {
    if (value > 1.0f) return 1.0f;
    if (value < -1.0f) return -1.0f;
    return value;
  }

  /**
   * @brief Set up low-pass filters for all channels
   *
   * Initializes anti-aliasing filters for each audio channel with appropriate
   * cutoff frequency (40% of Nyquist frequency) for the current sample rate.
   * This ensures proper anti-aliasing performance during DSD to PCM
   * conversion.
   */
  void setupTargetPCMRate() {
    TRACEI();

    // Initialize filters for the correct number of channels
    if (meta.sample_rate > 0 && meta.channels > 0) {
      float cutoffFreq =
          meta.sample_rate * meta.filter_cutoff;  // 40% of Nyquist frequency
      channelFilters.resize(meta.channels);
      for (int i = 0; i < meta.channels; i++) {
        channelFilters[i].begin(cutoffFreq, meta.sample_rate, meta.filter_q);
      }
    }
  }

  /**
   * @brief Calculate optimal decimation step for DSD to PCM conversion
   *
   * Calculates the decimation factor as the ratio of DSD sample rate to
   * target PCM sample rate. Clamps the value between 64 and 512 to ensure
   * reasonable processing efficiency and audio quality while maintaining good
   * anti-aliasing performance.
   */
  void setupDecimationStep() {
    TRACEI();
    if (meta.sample_rate == 0 || meta.dsd_sample_rate == 0) {
      LOGE("Invalid sample rates: DSD=%u, PCM=%u",
           (unsigned)meta.dsd_sample_rate, (unsigned)meta.sample_rate);
      return;
    }

    decimationStep = meta.dsd_sample_rate / meta.sample_rate;
    if (decimationStep < 64) {
      LOGW("Decimation step %u too low, setting to 64",
           (unsigned)decimationStep);
      decimationStep = 64;
    }
    if (decimationStep > 512) {
      LOGW("Decimation step %u too high, setting to 512",
           (unsigned)decimationStep);
      decimationStep = 512;
    }

    // Ensure decimation step is multiple of 8 for clean byte processing
    decimationStep = (decimationStep / 8) * 8;
    if (decimationStep < 64) decimationStep = 64;

    LOGI("Decimation step set to %u for DSD rate %u and target PCM rate %u",
         (unsigned)decimationStep, (unsigned)meta.dsd_sample_rate,
         (unsigned)meta.sample_rate);
  }

  /**
   * @brief Check if sufficient DSD data is available for conversion
   * @return true if enough data is buffered for one decimation step
   *
   * Determines if the DSD buffer contains enough data to perform one
   * decimation step of DSD to PCM conversion. For DSF format with byte
   * interleaving, we need enough bytes for all channels over the decimation
   * period.
   */
  bool hasEnoughData() {
    // DSF uses byte interleaving: each decimation step needs enough bytes
    // to cover all channels. Each byte contains 8 DSD samples for one
    // channel.
    int bytesPerDecimationStep = (decimationStep / 8) * meta.channels;
    if (bytesPerDecimationStep < meta.channels)
      bytesPerDecimationStep = meta.channels;

    return dsdBuffer.available() >= bytesPerDecimationStep;
  }

  /**
   * @brief Convert filtered DSD value to PCM sample in the buffer
   * @param filteredValue The filtered DSD value (range -1.0 to 1.0)
   * @param channel Channel index (0 for left/mono, 1 for right)
   */
  void writePCMSample(float filteredValue) {
    switch (meta.bits_per_sample) {
      case 8: {
        int8_t buffer8 = static_cast<int8_t>(filteredValue * 127.0f);
        pcmBuffer.write(buffer8);
        break;
      }
      case 16: {
        int16_t buffer16 = static_cast<int16_t>(filteredValue * 32767.0f);
        pcmBuffer.writeArray((uint8_t*)&buffer16, sizeof(int16_t));
        break;
      }
      case 24: {
        int24_t buffer24 =
            static_cast<int24_t>(filteredValue * 8388607.0f);  // 2^23 - 1
        pcmBuffer.writeArray((uint8_t*)&buffer24, sizeof(int24_t));
        break;
      }
      case 32: {
        int32_t buffer32 =
            static_cast<int32_t>(filteredValue * 2147483647.0f);  // 2^31 -
        pcmBuffer.writeArray((uint8_t*)&buffer32, sizeof(int32_t));
        break;
      }
      default:
        LOGE("Unsupported bits per sample: %d", meta.bits_per_sample);
        break;
    }
  }

  /**
   * @brief Find a specific tag within binary data
   * @param tag The tag string to search for (e.g., "fmt ", "data")
   * @param data The binary data buffer to search in
   * @param len The length of the data buffer
   * @return The position of the tag if found, -1 if not found
   *
   * Searches for DSF chunk identifiers within the file data. Used to locate
   * format and data sections within the DSF file structure.
   */
  int findTag(const char* tag, const uint8_t* data, size_t len) {
    int taglen = strlen(tag);
    uint32_t* pt;
    for (int j = 0; j < len - taglen; j++) {
      if (memcmp(tag, data + j, taglen) == 0) {
        return j;  // Found the tag at position j
      }
    }
    return -1;
  }

  /**
   * @brief Parse DSF format chunk to extract audio parameters
   * @param data Pointer to the fmt chunk data
   * @param len Length of available data
   * @return true if parsing was successful, false otherwise
   *
   * Extracts essential audio format information from the DSF format chunk,
   * including channel count, DSD sample rate, and validates the parameters
   * are within acceptable ranges for processing.
   */
  bool parseFMT(const uint8_t* data, size_t len) {
    TRACEI();
    if (len < sizeof(DSFFormat)) {
      LOGE("FMT section too short to parse DSF format header");
      return false;  // Not enough data to parse
    }
    DSFFormat* fmt = (DSFFormat*)data;
    meta.channels = fmt->channelNum;
    // Fallback to channel type if channels is 0
    if (meta.channels == 0) meta.channels = fmt->channelType;
    meta.dsd_sample_rate = fmt->samplingFrequency;

    // Validate channel count
    if (meta.channels == 0 || meta.channels > 8) {
      LOGE("Invalid channel count: %u (must be 1-8)", (unsigned)meta.channels);
      return false;
    }

    LOGI("channels: %u, DSD sample rate: %u", (unsigned)meta.channels,
         (unsigned)meta.dsd_sample_rate);
    return true;
  }

  /**
   * @brief Parse DSF data chunk to extract audio data information
   * @param data Pointer to the data chunk
   * @param len Length of available data
   * @return true if parsing was successful, false otherwise
   *
   * Extracts audio data size information and calculates estimated playback
   * duration and total PCM frames that will be produced after DSD to PCM
   * conversion is complete.
   */
  bool parseData(const uint8_t* data, size_t len) {
    TRACEI();
    if (len < sizeof(DSFDataHeader)) {
      LOGE("Data section too short to parse DSF data header");
      return false;  // Not enough data to parse
    }
    DSFDataHeader* header = (DSFDataHeader*)data;
    dataSize = header->chunkSize;
    meta.dsd_data_bytes = dataSize;

    uint64_t totalBits = dataSize * 8;
    uint64_t totalDSDSamples = totalBits / meta.channels;
    uint64_t totalPCMFrames =
        totalDSDSamples / (meta.dsd_sample_rate / meta.sample_rate);
    meta.pcm_frames = totalPCMFrames;
    meta.duration_sec = (float)totalPCMFrames / meta.sample_rate;
    return true;
  }
};

}  // namespace audio_tools
