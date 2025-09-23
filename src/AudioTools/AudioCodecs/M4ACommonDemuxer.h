#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "AudioTools/AudioCodecs/MP4Parser.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "MP4Parser.h"

namespace audio_tools {

/// The stsz sample size type should usually be uint32_t: However for audio
/// we expect that the sample size is usually aound  1 - 2k, so uint16_t
/// should be more then sufficient! Microcontolles only have a limited
/// amount of RAM, so this makes a big difference!
/// @brief Sample size type optimized for microcontrollers
/// @ingroup codecs
using stsz_sample_size_t = uint16_t;

/**
 * @brief Abstract base class for M4A/MP4 demuxers.
 * Provides shared functionality for both file-based and stream-based demuxers.
 */
class M4ACommonDemuxer {
 public:
  enum class Codec { Unknown, AAC, ALAC, MP3 };

  struct Frame {
    Codec codec;
    const char* mime = nullptr;
    const uint8_t* data;
    size_t size;
  };

  struct M4AAudioConfig {
    Codec codec = Codec::Unknown;  ///< Current codec.
    // aac
    int aacProfile = 2, sampleRateIdx = 4, channelCfg = 2;  ///< AAC config.
    // cookie
    Vector<uint8_t> alacMagicCookie;  ///< ALAC codec config.
  };

  /**
   * @brief A parser for the ESDS segment to extract the relevant aac
   * information.
   *
   */
  struct ESDSParser {
    uint8_t audioObjectType;
    uint8_t samplingRateIndex;
    uint8_t channelConfiguration;

    // Parses esds content to extract audioObjectType, frequencyIndex, and
    // channelConfiguration
    bool parse(const uint8_t* data, size_t size) {
      const uint8_t* ptr = data;
      const uint8_t* end = data + size;

      if (ptr + 4 > end) return false;
      ptr += 4;  // skip version + flags

      if (ptr >= end || *ptr++ != 0x03) return false;
      size_t es_len = parse_descriptor_length(ptr, end);
      if (ptr + es_len > end) return false;

      ptr += 2;  // skip ES_ID
      ptr += 1;  // skip flags

      if (ptr >= end || *ptr++ != 0x04) return false;
      size_t dec_len = parse_descriptor_length(ptr, end);
      if (ptr + dec_len > end) return false;

      ptr += 13;  // skip objectTypeIndication, streamType, bufferSizeDB,
                  // maxBitrate, avgBitrate

      if (ptr >= end || *ptr++ != 0x05) return false;
      size_t dsi_len = parse_descriptor_length(ptr, end);
      if (ptr + dsi_len > end || dsi_len < 2) return false;

      uint8_t byte1 = ptr[0];
      uint8_t byte2 = ptr[1];

      audioObjectType = (byte1 >> 3) & 0x1F;
      samplingRateIndex = ((byte1 & 0x07) << 1) | ((byte2 >> 7) & 0x01);
      channelConfiguration = (byte2 >> 3) & 0x0F;
      return true;
    }

   protected:
    // Helper to decode variable-length descriptor lengths (e.g. 0x80 80 80 05)
    inline size_t parse_descriptor_length(const uint8_t*& ptr,
                                          const uint8_t* end) {
      size_t len = 0;
      for (int i = 0; i < 4 && ptr < end; ++i) {
        uint8_t b = *ptr++;
        len = (len << 7) | (b & 0x7F);
        if ((b & 0x80) == 0) break;
      }
      return len;
    }
  };

  /**
   * @brief Extracts audio data based on the sample sizes defined in the stsz
   * box. It collects the data from the mdat box and calls the callback with the
   * extracted frames.
   */
  class SampleExtractor {
   public:
    using Frame = M4ACommonDemuxer::Frame;
    using Codec = M4ACommonDemuxer::Codec;
    using M4AAudioConfig = M4ACommonDemuxer::M4AAudioConfig;
    using FrameCallback = std::function<void(const Frame&, void*)>;

    /**
     * @brief Constructor. Initializes the extractor.
     */
    SampleExtractor(M4AAudioConfig& cfg) : audio_config{cfg} { begin(); }

    /**
     * @brief Resets the extractor state.
     */
    void begin() {
      sampleIndex = 0;
      buffer.clear();
      p_chunk_offsets->clear();
      p_sample_sizes->clear();
      buffer.resize(1024);
      current_size = 0;
      box_pos = 0;
      box_size = 0;
    }

    /**
     * @brief Sets the callback to be called for each extracted frame.
     * @param cb Callback function.
     */
    void setCallback(FrameCallback cb) { callback = cb; }

    /**
     * @brief Sets a reference pointer passed to the callback.
     * @param r Reference pointer.
     */
    void setReference(void* r) { ref = r; }

    /**
     * @brief Sets the maximum box size (e.g., for mdat). This is called before
     * the mdat data is posted. In order to be able to play a file multiple
     * times we just reset the sampleIndex!
     * @param size Maximum size in bytes.
     */
    void setMaxSize(size_t size) {
      box_size = size;
    }

    /**
     * @brief Writes data to the extractor, extracting frames as sample sizes
     * are met. Provides the data via the callback.
     * @param data Pointer to input data.
     * @param len Length of input data.
     * @param is_final True if this is the last chunk of the box.
     * @return Number of bytes processed.
     */
    size_t write(const uint8_t* data, size_t len, bool is_final) {
      // Resize buffer to the current sample size
      size_t currentSize = currentSampleSize();
      if (currentSize == 0) {
        LOGE("No sample size defined: e.g. mdat before stsz!");
        return 0;
      }
      resize(currentSize);

      /// fill buffer up to the current sample size
      for (int j = 0; j < len; j++) {
        assert(buffer.write(data[j]));
        if (buffer.available() >= currentSize) {
          LOGI("Sample# %zu: size %zu bytes", sampleIndex, currentSize);
          executeCallback(currentSize);
          buffer.clear();
          box_pos += currentSize;
          ++sampleIndex;
          currentSize = currentSampleSize();
          if (box_pos >= box_size) {
            LOGI("Reached end of box: %s write",
                 is_final ? "final" : "not final");
            return j;
          }
          if (currentSize == 0) {
            LOGE("No sample size defined, cannot write data");
            return j;
          }
        }
      }
      return len;
    }

    /**
     * @brief Returns the buffer of sample sizes.
     * @return Reference to the buffer of sample sizes.
     */
    BaseBuffer<stsz_sample_size_t>& getSampleSizesBuffer() {
      return *p_sample_sizes;
    }

    /**
     * @brief Sets the buffer to use for sample sizes.
     * @param buffer Reference to the buffer to use.
     */
    void setSampleSizesBuffer(BaseBuffer<stsz_sample_size_t>& buffer) {
      p_sample_sizes = &buffer;
    }

    /**
     * @brief Returns the buffer of chunk offsets.
     * @return Reference to the buffer of chunk offsets.
     */
    BaseBuffer<uint32_t>& getChunkOffsetsBuffer() { return *p_chunk_offsets; }

    /**
     * @brief Sets the buffer to use for chunk offsets.
     * @param buffer Reference to the buffer to use.
     */
    void setChunkOffsetsBuffer(BaseBuffer<uint32_t>& buffer) {
      p_chunk_offsets = &buffer;
    }

    /**
     * @brief Sets a fixed sample size/count instead of using the sampleSizes
     * table.
     * @param sampleSize Size of each sample.
     * @param sampleCount Number of samples.
     */
    void setFixedSampleCount(uint32_t sampleSize, uint32_t sampleCount) {
      fixed_sample_size = sampleSize;
      fixed_sample_count = sampleCount;
    }

    /**
     * @brief Constructs a Frame object for the current codec.
     * @param size Size of the frame.
     * @param buffer SingleBuffer with data.
     * @return Frame object.
     */
    Frame getFrame(size_t size, SingleBuffer<uint8_t>& buffer) {
      Frame frame;
      frame.codec = audio_config.codec;
      frame.data = buffer.data();
      frame.size = size;
      switch (audio_config.codec) {
        case Codec::AAC: {
          // Prepare ADTS header + AAC frame
          tmp.resize(size + 7);
          writeAdtsHeader(tmp.data(), audio_config.aacProfile,
                          audio_config.sampleRateIdx, audio_config.channelCfg,
                          size);
          memcpy(tmp.data() + 7, buffer.data(), size);
          frame.data = tmp.data();
          frame.size = size + 7;
          frame.mime = "audio/aac";
          break;
        }
        case Codec::ALAC:
          frame.mime = "audio/alac";
          break;
        case Codec::MP3:
          frame.mime = "audio/mpeg";
          break;
        default:
          frame.mime = nullptr;
          break;
      }
      return frame;
    }

   protected:
    M4AAudioConfig& audio_config;
    SingleBuffer<stsz_sample_size_t>
        defaultSampleSizes;                      ///< Table of sample sizes.
    SingleBuffer<uint32_t> defaultChunkOffsets;  ///< Table of chunk offsets.
    BaseBuffer<stsz_sample_size_t>* p_sample_sizes = &defaultSampleSizes;
    BaseBuffer<uint32_t>* p_chunk_offsets = &defaultChunkOffsets;
    Vector<uint8_t> tmp;
    FrameCallback callback = nullptr;  ///< Frame callback.
    void* ref = nullptr;               ///< Reference pointer for callback.
    size_t sampleIndex = 0;            ///< Current sample index.
    SingleBuffer<uint8_t> buffer;      ///< Buffer for accumulating sample data.
    uint32_t fixed_sample_size = 0;    ///< Fixed sample size (if used).
    uint32_t fixed_sample_count = 0;   ///< Fixed sample count (if used).
    size_t current_size = 0;           ///< Current sample size.
    size_t box_size = 0;               ///< Maximum size of the current sample.
    size_t box_pos = 0;                ///< Current position in the box.

    /**
     * @brief Executes the callback for a completed frame.
     * @param size Size of the frame.
     */
    void executeCallback(size_t size) {
      Frame frame = getFrame(size, buffer);
      if (callback)
        callback(frame, ref);
      else
        LOGE("No callback defined for audio frame extraction");
    }

    /**
     * @brief Resizes the internal buffer if needed.
     * @param newSize New buffer size.
     */
    void resize(size_t newSize) {
      if (buffer.size() < newSize) {
        buffer.resize(newSize);
      }
    }

    /**
     * @brief Returns the current sample size.
     * @return Size of the current sample.
     */
    size_t currentSampleSize() {
      static size_t last_index = -1;
      static size_t last_size = -1;

      // Return cached size
      if (sampleIndex == last_index) {
        return last_size;
      }

      // using fixed sizes w/o table
      if (fixed_sample_size > 0 && fixed_sample_count > 0 &&
          sampleIndex < fixed_sample_count) {
        return fixed_sample_size;
      }
      stsz_sample_size_t nextSize = 0;
      if (p_sample_sizes->read(nextSize)) {
        last_index = sampleIndex;
        last_size = nextSize;
        return nextSize;
      }
      return 0;
    }

    /**
     * @brief Writes an ADTS header for an AAC frame.
     * @param adts Output buffer for the header.
     * @param aacProfile AAC profile.
     * @param sampleRateIdx Sample rate index.
     * @param channelCfg Channel configuration.
     * @param frameLen Frame length.
     */
    static void writeAdtsHeader(uint8_t* adts, int aacProfile,
                                int sampleRateIdx, int channelCfg,
                                int frameLen) {
      adts[0] = 0xFF;
      adts[1] = 0xF1;
      adts[2] = ((aacProfile - 1) << 6) | (sampleRateIdx << 2) |
                ((channelCfg >> 2) & 0x1);
      adts[3] = ((channelCfg & 0x3) << 6) | ((frameLen + 7) >> 11);
      adts[4] = ((frameLen + 7) >> 3) & 0xFF;
      adts[5] = (((frameLen + 7) & 0x7) << 5) | 0x1F;
      adts[6] = 0xFC;
    }
  };

  using FrameCallback = std::function<void(const Frame&, void* ref)>;

  M4ACommonDemuxer() = default;
  virtual ~M4ACommonDemuxer() = default;

  /**
   * @brief Sets the callback for extracted audio frames.
   * @param cb Frame callback function.
   */
  virtual void setCallback(FrameCallback cb) { frame_callback = cb; }
  /**
   * @brief Sets the buffer to use for sample sizes.
   * @param buffer Reference to the buffer to use.
   */
  void setSampleSizesBuffer(BaseBuffer<stsz_sample_size_t>& buffer) {
    sampleExtractor.setSampleSizesBuffer(buffer);
  }
  /**
   * @brief Sets the buffer to use for sample sizes.
   * @param buffer Reference to the buffer to use.
   */
  void setChunkOffsetsBuffer(BaseBuffer<uint32_t>& buffer) {
    sampleExtractor.setChunkOffsetsBuffer(buffer);
  }

  void begin() {
    stsz_processed = false;
    stco_processed = false;
    audio_config.alacMagicCookie.clear();
    audio_config.codec = Codec::Unknown;
    parser.begin();
    sampleExtractor.begin();
    chunk_offsets_count = 0;
    sample_count = 0;
  }
  /**
   * @brief Sets the AAC configuration for ADTS header generation.
   * @param profile AAC profile.
   * @param srIdx Sample rate index.
   * @param chCfg Channel configuration.
   */
  void setAACConfig(int profile, int srIdx, int chCfg) {
    audio_config.aacProfile = profile;
    audio_config.sampleRateIdx = srIdx;
    audio_config.channelCfg = chCfg;
  }

  void setM4AAudioConfig(M4AAudioConfig cfg) { audio_config = cfg; }

  M4AAudioConfig getM4AAudioConfig() { return audio_config; }

  void resize(int size) {
    default_size = size;
    if (buffer.size() < size) {
      buffer.resize(size);
    }
  }

  /// File offset of stsz box
  uint32_t getStszFileOffset() const {
    return stsz_offset;
  }

  /// samples in stsz
  uint32_t getSampleCount() const {
    return sample_count;
  }

  virtual void setupParser() = 0;

 protected:
  FrameCallback frame_callback = nullptr;
  SampleExtractor sampleExtractor{
      audio_config};            ///< Extractor for audio samples.
  MP4Parser parser;  ///< Underlying MP4 parser.
  bool stsz_processed = false;  ///< Marks the stsz table as processed
  bool stco_processed = false;  ///< Marks the stco table as processed
  bool stsd_processed = false;
  M4AAudioConfig audio_config;
  SingleBuffer<uint8_t> buffer;  ///< Buffer for incremental data.
  uint32_t sample_count = 0;     ///< Number of samples in stsz
  uint32_t stsz_offset = 0;
  uint32_t chunk_offsets_count = 0;
  size_t default_size = 2 * 1024;  ///< Default buffer size.

  /**
   * @brief Reads a 32-bit big-endian unsigned integer from a buffer.
   * @param p Pointer to buffer.
   * @return 32-bit unsigned integer.
   */
  static uint32_t readU32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
  }

  static uint32_t readU32(const uint32_t num) {
    uint8_t* p = (uint8_t*)&num;
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
  }

   uint32_t readU32Buffer() {
      uint32_t nextSize = 0;
      buffer.readArray((uint8_t*)&nextSize, 4);
      return readU32(nextSize);
  }
 

  /**
   * @brief Checks if the buffer at the given offset matches the specified type.
   * @param buffer Pointer to the buffer.
   * @param type 4-character type string (e.g. "mp4a").
   * @param offset Offset in the buffer to check.
   * @return true if the type matches, false otherwise.
   */
  bool checkType(uint8_t* buffer, const char* type, int offset) {
    if (buffer == nullptr || type == nullptr) return false;
    bool result = buffer[offset] == type[0] && buffer[offset + 1] == type[1] &&
                  buffer[offset + 2] == type[2] &&
                  buffer[offset + 3] == type[3];
    return result;
  }

  void onStsd(const MP4Parser::Box& box) {
    LOGI("Box: %s, size: %u bytes", box.type, (unsigned)box.available);
    if (box.seq == 0) {
      resize(box.size);
      buffer.clear();
    }

    buffer.writeArray(box.data, box.data_size);

    if (box.is_complete && buffer.available() >= 8) {
      // printHexDump(box);
      uint32_t entryCount = readU32(buffer.data() + 4);
      // One or more sample entry boxes (e.g. mp4a, .mp3, alac)
      parser.parseString(buffer.data() + 8, box.data_size - 8,
                         box.file_offset + 8 + 8, box.level + 1);
      buffer.clear();
    }
  }

  /**
   * @brief Handles the mp4a box.
   * @param box MP4 box.
   */
  void onMp4a(const MP4Parser::Box& box) {
    LOGI("onMp4a: %s, size: %zu bytes", box.type, box.data_size);

    if (box.is_complete) {
      // printHexDump(box);

      // use default configuration
      int aacProfile = 2;     // Default: AAC LC
      int sampleRateIdx = 4;  // Default: 44100 Hz
      int channelCfg = 2;     // Default: Stereo
      setAACConfig(aacProfile, sampleRateIdx, channelCfg);
      audio_config.codec = Codec::AAC;

      /// for mp4a we expect to contain a esds: child boxes start at 36
      int pos = 36 - 8;
      parser.parseString(box.data + pos, box.data_size - pos, box.level + 1);
    }
  }

  /**
   * @brief Handles the esds (Elementary Stream Descriptor) box.
   * @param box MP4 box.
   */
  void onEsds(const MP4Parser::Box& box) {
    LOGI("onEsds: %s, size: %zu bytes", box.type, box.data_size);
    // printHexDump(box);
    ESDSParser esdsParser;
    if (!esdsParser.parse(box.data, box.data_size)) {
      LOGE("Failed to parse esds box");
      return;
    }
    LOGI(
        "-> esds: AAC objectType: %u, samplingRateIdx: %u, "
        "channelCfg: %u",
        esdsParser.audioObjectType, esdsParser.samplingRateIndex,
        esdsParser.channelConfiguration);
    setAACConfig(esdsParser.audioObjectType, esdsParser.samplingRateIndex,
                 esdsParser.channelConfiguration);
  }

  // void fixALACMagicCookie(uint8_t* cookie, size_t len) {
  //   if (len < 28) {
  //     return;
  //   }

  //   // Helper to read/write big-endian
  //   auto read32 = [](uint8_t* p) -> uint32_t {
  //     return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
  //   };
  //   auto write32 = [](uint8_t* p, uint32_t val) {
  //     p[0] = (val >> 24) & 0xFF;
  //     p[1] = (val >> 16) & 0xFF;
  //     p[2] = (val >> 8) & 0xFF;
  //     p[3] = val & 0xFF;
  //   };
  //   auto read16 = [](uint8_t* p) -> uint16_t { return (p[0] << 8) | p[1]; };
  //   auto write16 = [](uint8_t* p, uint16_t val) {
  //     p[0] = (val >> 8) & 0xFF;
  //     p[1] = val & 0xFF;
  //   };

  //   // Fix values if zero or invalid
  //   if (read32(cookie + 0) == 0) write32(cookie + 0, 4096);    // frameLength
  //   if (cookie[6] == 0) cookie[6] = 16;                        // bitDepth
  //   if (cookie[7] == 0 || cookie[7] > 32) cookie[7] = 10;      // pb
  //   if (cookie[8] == 0 || cookie[8] > 32) cookie[8] = 14;      // mb
  //   if (cookie[9] == 0 || cookie[9] > 32) cookie[9] = 10;      // kb
  //   if (cookie[10] == 0 || cookie[10] > 8) cookie[10] = 2;     // numChannels
  //   if (read16(cookie + 11) == 0) write16(cookie + 11, 255);   // maxRun
  //   if (read32(cookie + 13) == 0) write32(cookie + 13, 8192);  // maxFrameBytes
  //   if (read32(cookie + 17) == 0) write32(cookie + 17, 512000);  // avgBitRate
  //   if (read32(cookie + 21) == 0) write32(cookie + 21, 44100);   // sampleRate
  // }

  /**
   * @brief Handles the alac box.
   * @param box MP4 box.
   */
  void onAlac(const MP4Parser::Box& box) {
    LOGI("onAlac: %s, size: %zu bytes", box.type, box.data_size);
    audio_config.codec = Codec::ALAC;

    // only alac box in alac contains magic cookie
    MP4Parser::Box alac;
    if (parser.findBox("alac", box.data, box.data_size, alac)) {
      // fixALACMagicCookie((uint8_t*)alac.data, alac.data_size);
      audio_config.alacMagicCookie.resize(alac.data_size - 4);
      std::memcpy(audio_config.alacMagicCookie.data(), alac.data + 4,
                  alac.data_size - 4);
    }
  }

  /**
   * @brief Handles the stsz (Sample Size) box.
   * @param box MP4 box.
   */
  void onStsz(MP4Parser::Box& box) {
    MP4Parser::defaultCallback(box,0);
    LOGI("onStsz #%u: %s, size: %u of %u bytes", (unsigned) box.seq, box.type, (unsigned) box.available, (unsigned) box.data_size);
    if (stsz_processed) return;
    BaseBuffer<stsz_sample_size_t>& sampleSizes =
        sampleExtractor.getSampleSizesBuffer();

    buffer.resize(box.available);
    size_t written = buffer.writeArray(box.data, box.available);
    assert(written = box.available);

    // get sample count and size from the box
    if (sample_count == 0 && buffer.available() > 12) {
      readU32Buffer();  // skip version + flags
      uint32_t sampleSize = readU32Buffer();
      uint32_t sampleCount = readU32Buffer();
      sample_count = sampleCount;
      stsz_offset = box.file_offset;

      sampleSizes.resize(sample_count);
      if (sampleSize != 0) {
        sampleExtractor.setFixedSampleCount(sampleSize, sampleCount);
      }
    }

    // incrementally process sampleSize
    int count = 0;
    while (buffer.available() >= 4) {
      stsz_sample_size_t sampleSize = readU32Buffer();
      assert(sampleSizes.write(sampleSize));
      count += 4;
    }
    // Remove processed data
    buffer.trim();

    if (box.is_complete) {
      stsz_processed = true;
    }
  }

  // /**
  //  * @brief Handles the stco (Chunk Offset) box.
  //  * @param box MP4 box.
  //  */
  // void onStco(MP4Parser::Box& box) {
  //   LOGI("onStco: %s, size: %zu bytes", box.type, box.data_size);
  //   if (stco_processed) return;
  //   BaseBuffer<uint32_t>& chunkOffsets =
  //       sampleExtractor.getChunkOffsetsBuffer();

  //   buffer.resize(box.available);
  //   buffer.writeArray(box.data, box.available);

  //   // get chunk_offsets_count from the box
  //   if (chunk_offsets_count == 0 && buffer.available() > 12) {
  //     chunk_offsets_count = readU32(buffer.data());
  //     buffer.clearArray(4);  // clear version + flags
  //   }

  //   // incrementally process sampleSize
  //   int j = 0;
  //   for (j = 0; j < buffer.available(); j += 4) {
  //     uint32_t sampleSize = readU32(buffer.data() + j);
  //     chunkOffsets.write(sampleSize);
  //   }
  //   buffer.clearArray(j);

  //   if (box.is_complete) {
  //     stco_processed = true;
  //   }
  // }

  void printHexDump(const MP4Parser::Box& box) {
    const uint8_t* data = box.data;
    size_t len = box.data_size;
    LOGI("===========================");
    for (size_t i = 0; i < len; i += 16) {
      char hex[49] = {0};
      char ascii[17] = {0};
      for (size_t j = 0; j < 16 && i + j < len; ++j) {
        sprintf(hex + j * 3, "%02X ", data[i + j]);
        ascii[j] = (data[i + j] >= 32 && data[i + j] < 127) ? data[i + j] : '.';
      }
      ascii[16] = 0;
      LOGI("%04zx: %-48s |%s|", i, hex, ascii);
    }
    LOGI("===========================");
  }
};

}  // namespace audio_tools