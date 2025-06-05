#pragma once

#include "AudioTools/AudioCodecs/MP4ParserIncremental.h"

namespace audio_tools {

/// The stsz sample size type should usually be uint32_t: However for audio
/// we expect that the sample size is usually aound  1 - 2k, so uint16_t
/// should be more then sufficient! Microcontolles only have a limited
/// amount of RAM, so this makes a big difference!
using stsz_sample_size_t = uint16_t;

/**
 * @brief A simple M4A audio data demuxer which is providing
 * AAC, MP3 and ALAC frames.
 */
class M4AAudioDemuxer {
 public:
  /**
   * @brief Supported codecs.
   */
  enum class Codec { AAC, ALAC, MP3, Unknown };

  /**
   * @brief Represents a frame of audio data with codec, mime type, data and
   * size.
   */
  struct Frame {
    Codec codec;                 ///< Codec type.
    const char* mime = nullptr;  ///< MIME type string.
    const uint8_t* data;         ///< Pointer to frame data.
    size_t size;                 ///< Size of frame data in bytes.
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
    bool isValid = false;  ///< True if the ESDP is valid

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
    using Frame = M4AAudioDemuxer::Frame;
    using Codec = M4AAudioDemuxer::Codec;
    using FrameCallback = std::function<void(const Frame&, void*)>;

    /**
     * @brief Constructor. Initializes the extractor.
     */
    SampleExtractor() { begin(); }

    /**
     * @brief Resets the extractor state.
     */
    void begin() {
      sampleIndex = 0;
      buffer.clear();
      sampleSizes.clear();
      buffer.resize(1024);
      current_size = 0;
      box_pos = 0;
      box_size = 0;
    }

    /**
     * @brief Sets the codec for extraction.
     * @param c Codec type.
     */
    void setCodec(M4AAudioDemuxer::Codec c) { codec = c; }

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
      sampleIndex = 0;
    }

    /**
     * @brief Writes data to the extractor, extracting frames as sample sizes
     * are met.
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
        buffer.write(data[j]);
        if (buffer.available() == currentSize) {
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
          resize(currentSize);
        }
      }
      return len;
    }

    /**
     * @brief Returns the vector of sample sizes.
     * @return Reference to the vector of sample sizes.
     */
    Vector<stsz_sample_size_t>& getSampleSizes() { return sampleSizes; }

    /**
     * @brief Returns the vector of chunk offsets.
     * @return Reference to the vector of chunk offsets.
     */
    Vector<uint32_t>& getChunkOffsets() { return chunkOffsets; }

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
     * @brief Sets the AAC configuration for ADTS header generation.
     * @param profile AAC profile.
     * @param srIdx Sample rate index.
     * @param chCfg Channel configuration.
     */
    void setAACConfig(int profile, int srIdx, int chCfg) {
      aacProfile = profile;
      sampleRateIdx = srIdx;
      channelCfg = chCfg;
    }

   protected:
    Vector<stsz_sample_size_t> sampleSizes;  ///< Table of sample sizes.
    Vector<uint32_t> chunkOffsets;           ///< Table of chunk offsets.
    Codec codec = Codec::Unknown;            ///< Current codec.
    FrameCallback callback = nullptr;        ///< Frame callback.
    void* ref = nullptr;           ///< Reference pointer for callback.
    size_t sampleIndex = 0;        ///< Current sample index.
    SingleBuffer<uint8_t> buffer;  ///< Buffer for accumulating sample data.
    int aacProfile = 2, sampleRateIdx = 4, channelCfg = 2;  ///< AAC config.
    uint32_t fixed_sample_size = 0;   ///< Fixed sample size (if used).
    uint32_t fixed_sample_count = 0;  ///< Fixed sample count (if used).
    size_t current_size = 0;          ///< Current sample size.
    size_t box_size = 0;              ///< Maximum size of the current sample.
    size_t box_pos = 0;               ///< Current position in the box.

    /**
     * @brief Executes the callback for a completed frame.
     * @param size Size of the frame.
     */
    void executeCallback(size_t size) {
      size_t frameSize = size;
      Frame frame;
      frame.codec = codec;
      frame.data = buffer.data();
      frame.size = frameSize;
      switch (codec) {
        case Codec::AAC: {
          uint8_t out[frameSize + 7];
          writeAdtsHeader(out, aacProfile, sampleRateIdx, channelCfg,
                          frameSize);
          memcpy(out + 7, buffer.data(), frameSize);
          frame.data = out;
          frame.size = sizeof(out);
          frame.mime = "audio/aac";
          if (callback)
            callback(frame, ref);
          else
            LOGE("No callback defined for audio frame extraction");
          return;
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
      // using fixed sizes w/o table
      if (fixed_sample_size > 0 && fixed_sample_count > 0 &&
          sampleIndex < fixed_sample_count) {
        return fixed_sample_size;
      }
      if (sampleSizes && sampleIndex < sampleSizes.size()) {
        return sampleSizes[sampleIndex];
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

  /**
   * @brief Constructor. Sets up parser callbacks.
   */
  M4AAudioDemuxer() {
    // global box data callback to get sizes
    parser.setReference(this);
    parser.setCallback(boxDataSetupCallback);

    // incremental data callback
    parser.setIncrementalDataCallback(incrementalBoxDataCallback);

    // parsing for content of stsd (Sample Description Box)
    parser.setCallback("esds", [](MP4Parser::Box& box, void* ref) {
      static_cast<M4AAudioDemuxer*>(ref)->onEsds(box);
    });
    parser.setCallback("mp4a", [](MP4Parser::Box& box, void* ref) {
      static_cast<M4AAudioDemuxer*>(ref)->onMp4a(box);
    });
    parser.setCallback("alac", [](MP4Parser::Box& box, void* ref) {
      static_cast<M4AAudioDemuxer*>(ref)->onAlac(box);
    });
  }

  /**
   * @brief Defines the callback that returns the audio frames.
   * @param cb Frame callback function.
   */
  void setCallback(FrameCallback cb) {
    sampleExtractor.setReference(ref);
    sampleExtractor.setCallback(cb);
  }

  /**
   * @brief Initializes the demuxer and resets state.
   */
  void begin() {
    codec = Codec::Unknown;
    alacMagicCookie.clear();
    resize(default_size);

    stsz_processed = false;
    stco_processed = false;

    // When codec/sampleSizes/callback/ref change, update the extractor:
    parser.begin();
    sampleExtractor.begin();
  }

  /**
   * @brief Writes data to the demuxer for parsing.
   * @param data Pointer to input data.
   * @param len Length of input data.
   */
  void write(const uint8_t* data, size_t len) { parser.write(data, len); }

  /**
   * @brief Returns the available space for writing.
   * @return Number of bytes available for writing.
   */
  int availableForWrite() { return parser.availableForWrite(); }

  /**
   * @brief Returns the ALAC magic cookie (codec config).
   * @return Reference to the ALAC magic cookie vector.
   */
  Vector<uint8_t>& getALACMagicCookie() { return alacMagicCookie; }

  /**
   * @brief Sets a reference pointer for callbacks.
   * @param ref Reference pointer.
   */
  void setReference(void* ref) { this->ref = ref; }

  /**
   * @brief Resizes the internal buffer.
   * @param size New buffer size.
   */
  void resize(int size) {
    default_size = size;
    if (buffer.size() < size) {
      buffer.resize(size);
    }
  }

 protected:
  MP4ParserIncremental parser;      ///< Underlying MP4 parser.
  Codec codec = Codec::Unknown;     ///< Current codec.
  Vector<uint8_t> alacMagicCookie;  ///< ALAC codec config.
  SingleBuffer<uint8_t> buffer;     ///< Buffer for incremental data.
  SampleExtractor sampleExtractor;  ///< Extractor for audio samples.
  void* ref = nullptr;              ///< Reference pointer for callbacks.
  size_t default_size = 2 * 1024;   ///< Default buffer size.
  bool stsz_processed = false;      ///< Marks the stsz table as processed
  bool stco_processed = false;      ///< Marks the stco table as processed

  /**
   * @brief Reads a 32-bit big-endian unsigned integer from a buffer.
   * @param p Pointer to buffer.
   * @return 32-bit unsigned integer.
   */
  static uint32_t readU32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
  }

  /**
   * @brief Checks if a box type is relevant for audio demuxing.
   * @param type Box type string.
   * @return True if relevant, false otherwise.
   */
  static bool isRelevantBox(const char* type) {
    // Check if the box is relevant for audio demuxing
    return (StrView(type) == "stsd" || StrView(type) == "stsz" ||
            StrView(type) == "stco");
  }

  /**
   * @brief Callback for box data setup.
   * @param box MP4 box.
   * @param ref Reference pointer.
   */
  static void boxDataSetupCallback(MP4Parser::Box& box, void* ref) {
    M4AAudioDemuxer& self = *static_cast<M4AAudioDemuxer*>(ref);

    // mdat must not be buffered
    if (StrView(box.type) == "mdat") {
      LOGI("Box: %s, size: %u bytes", box.type, (unsigned)box.size);
      self.sampleExtractor.setMaxSize(box.size);
      return;
    }

    bool is_relevant = isRelevantBox(box.type);
    if (is_relevant) {
      LOGI("Box: %s, size: %u bytes", box.type, (unsigned)box.size);
      if (box.data_size == 0) {
        // setup for incremental processing
        self.resize(box.size);
        self.buffer.clear();
      } else {
        // we have the complete box data
        self.processBox(box);
      }
    }
  }

  /**
   * @brief Callback for incremental box data.
   * @param box MP4 box.
   * @param data Pointer to data.
   * @param len Length of data.
   * @param is_final True if this is the last chunk.
   * @param ref Reference pointer.
   */
  static void incrementalBoxDataCallback(MP4Parser::Box& box,
                                         const uint8_t* data, size_t len,
                                         bool is_final, void* ref) {
    M4AAudioDemuxer& self = *static_cast<M4AAudioDemuxer*>(ref);

    // mdat must not be buffered
    if (StrView(box.type) == "mdat") {
      LOGI("*Box: %s, size: %u bytes", box.type, (unsigned)len);
      self.sampleExtractor.write(data, len, is_final);
      return;
    }

    // only process relevant boxes
    if (!isRelevantBox(box.type)) return;

    LOGI("*Box: %s, size: %u bytes", box.type, (unsigned)len);

    // others fill buffer incrementally
    if (len > 0) {
      size_t written = self.buffer.writeArray(data, len);
      if (written != len) {
        LOGE("Failed to write all data to buffer, written: %zu, expected: %zu",
             written, len);
      }
    }

    // on last chunk, call the specific box handler
    if (is_final) {
      MP4Parser::Box complete_box = box;
      complete_box.data = self.buffer.data();
      complete_box.data_size = self.buffer.size();
      self.processBox(complete_box);
      // The buffer might be quite large, so we resize it to the default size
      self.resize(self.default_size);
    }
  }

  /**
   * @brief Processes a parsed MP4 box.
   * @param box MP4 box.
   */
  void processBox(MP4Parser::Box& box) {
    if (StrView(box.type) == "stsd") {
      onStsd(box);
    } else if (StrView(box.type) == "stsz") {
      onStsz(box);
    } else if (StrView(box.type) == "stco") {
      // onStco(box); // currently not supported
    }
  }

  /**
   * @brief Handles the stsd (Sample Description) box.
   * @param box MP4 box.
   */
  void onStsd(const MP4Parser::Box& box) {
    LOGI("onStsd: %s, size: %zu bytes", box.type, box.data_size);
    // printHexDump(box);
    if (box.data_size < 8) return;
    uint32_t entryCount = readU32(box.data + 4);
    // One or more sample entry boxes (e.g. mp4a, .mp3, alac)
    parser.parseString(box.data + 8, box.data_size - 8, box.file_offset + 8 + 8,
                       box.level + 1);
  }

  /**
   * @brief Handles the mp4a box.
   * @param box MP4 box.
   */
  void onMp4a(const MP4Parser::Box& box) {
    LOGI("onMp4a: %s, size: %zu bytes", box.type, box.data_size);
    // printHexDump(box);
    if (box.data_size < 36) return;  // Minimum size for mp4a box

    // use default configuration
    int aacProfile = 2;     // Default: AAC LC
    int sampleRateIdx = 4;  // Default: 44100 Hz
    int channelCfg = 2;     // Default: Stereo
    sampleExtractor.setAACConfig(aacProfile, sampleRateIdx, channelCfg);
    codec = Codec::AAC;
    sampleExtractor.setCodec(codec);

    /// for mp4a we expect to contain a esds: child boxes start at 36
    int pos = 36 - 8;
    parser.parseString(box.data + pos, box.data_size - pos, box.level + 1);
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
    sampleExtractor.setAACConfig(esdsParser.audioObjectType,
                                 esdsParser.samplingRateIndex,
                                 esdsParser.channelConfiguration);
  }

  void fixALACMagicCookie(uint8_t* cookie, size_t len) {
    if (len < 28) {
      return;
    }

    // Helper to read/write big-endian
    auto read32 = [](uint8_t* p) -> uint32_t {
      return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    };
    auto write32 = [](uint8_t* p, uint32_t val) {
      p[0] = (val >> 24) & 0xFF;
      p[1] = (val >> 16) & 0xFF;
      p[2] = (val >> 8) & 0xFF;
      p[3] = val & 0xFF;
    };
    auto read16 = [](uint8_t* p) -> uint16_t { return (p[0] << 8) | p[1]; };
    auto write16 = [](uint8_t* p, uint16_t val) {
      p[0] = (val >> 8) & 0xFF;
      p[1] = val & 0xFF;
    };

    // Fix values if zero or invalid
    if (read32(cookie + 0) == 0) write32(cookie + 0, 4096);    // frameLength
    if (cookie[6] == 0) cookie[6] = 16;                        // bitDepth
    if (cookie[7] == 0 || cookie[7] > 32) cookie[7] = 10;      // pb
    if (cookie[8] == 0 || cookie[8] > 32) cookie[8] = 14;      // mb
    if (cookie[9] == 0 || cookie[9] > 32) cookie[9] = 10;      // kb
    if (cookie[10] == 0 || cookie[10] > 8) cookie[10] = 2;     // numChannels
    if (read16(cookie + 11) == 0) write16(cookie + 11, 255);   // maxRun
    if (read32(cookie + 13) == 0) write32(cookie + 13, 8192);  // maxFrameBytes
    if (read32(cookie + 17) == 0) write32(cookie + 17, 512000);  // avgBitRate
    if (read32(cookie + 21) == 0) write32(cookie + 21, 44100);   // sampleRate
  }

  /**
   * @brief Handles the alac box.
   * @param box MP4 box.
   */
  void onAlac(const MP4Parser::Box& box) {
    LOGI("onAlac: %s, size: %zu bytes", box.type, box.data_size);
    codec = Codec::ALAC;
    sampleExtractor.setCodec(codec);

    // only alac box in alac contains magic cookie
    MP4Parser::Box alac;
    if (parser.findBox("alac", box.data, box.data_size, alac)) {
      // fixALACMagicCookie((uint8_t*)alac.data, alac.data_size);
      alacMagicCookie.resize(alac.data_size - 4);
      std::memcpy(alacMagicCookie.data(), alac.data + 4, alac.data_size - 4);
    }
  }

  /**
   * @brief Handles the stsz (Sample Size) box.
   * @param box MP4 box.
   */
  void onStsz(MP4Parser::Box& box) {
    LOGI("onStsz: %s, size: %zu bytes", box.type, box.data_size);
    if (stsz_processed) return;
    // Parse stsz box and fill sampleSizes
    const uint8_t* data = box.data;
    uint32_t sampleSize = readU32(data + 4);
    uint32_t sampleCount = readU32(data + 8);
    sampleExtractor.begin();
    Vector<stsz_sample_size_t>& sampleSizes = sampleExtractor.getSampleSizes();
    if (sampleSize == 0) {
      LOGI("-> Sample Sizes Count: %u", sampleCount);
      sampleSizes.resize(sampleCount);
      for (uint32_t i = 0; i < sampleCount; ++i) {
        uint32_t sampleSizes32 = readU32(data + 12 + i * 4);
        sampleSizes[i] = static_cast<stsz_sample_size_t>(sampleSizes32);
        assert(static_cast<uint32_t>(sampleSizes[i]) == sampleSizes32);
      }
    } else {
      sampleExtractor.setFixedSampleCount(sampleSize, sampleCount);
    }
    stsz_processed = true;
  }

  /**
   * @brief Handles the stco (Chunk Offset) box.
   * @param box MP4 box.
   */
  void onStco(MP4Parser::Box& box) {
    LOGI("onStco: %s, size: %zu bytes", box.type, box.data_size);
    if (stco_processed) return;
    // Parse stco box and fill chunkOffsets
    const uint8_t* data = box.data + 4;
    size_t size = box.data_size;
    if (size < 4) return;
    uint32_t entryCount = readU32(data);
    Vector<uint32_t>& chunkOffsets = sampleExtractor.getChunkOffsets();
    if (size < 4 + 4 * entryCount) return;
    chunkOffsets.resize(entryCount);
    LOGI("-> Chunk offsets count: %u", entryCount);
    for (uint32_t i = 0; i < entryCount; ++i) {
      chunkOffsets[i] = readU32(data + 4 + i * 4);
    }
    stco_processed = true;
  }

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