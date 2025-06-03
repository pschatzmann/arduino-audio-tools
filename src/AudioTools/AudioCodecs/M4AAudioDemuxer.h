#pragma once

#include "AudioTools/AudioCodecs/MP4ParserIncremental.h"

namespace audio_tools {

/**
 * @brief A simple M4A audio data demuxer which is providing
 * AAC, MP3 and ALAC frames.
 */
class M4AAudioDemuxer {
 public:
  enum class Codec { AAC, ALAC, MP3, Unknown };

  /***
   * @brief Represents a frame of audio data with codec, mime type, data and
   * size.
   */
  struct Frame {
    Codec codec;
    const char* mime = nullptr;
    const uint8_t* data;
    size_t size;
    uint64_t timestamp;
  };

  /***
   * @brief Extracts audio data based on the sample sizes defined in the stsz
   * box. It collects the data from the mdat box and calls the callback with the
   * extracted frames.
   */
  class SampleExtractor {
   public:
    using Frame = M4AAudioDemuxer::Frame;
    using Codec = M4AAudioDemuxer::Codec;
    using FrameCallback = std::function<void(const Frame&, void*)>;

    SampleExtractor() { begin(); }

    void begin() {
      sampleIndex = 0;
      buffer.clear();
      sampleSizes.clear();
      buffer.resize(1024);
      current_size = 0;
      box_pos = 0;
      box_size = 0;
    }

    void setCodec(M4AAudioDemuxer::Codec c) { codec = c; }

    void setCallback(FrameCallback cb) { callback = cb; }

    void setReference(void* r) { ref = r; }

    /// box size e.g. of mdat
    void setMaxSize(size_t size) { box_size = size; }

    size_t write(const uint8_t* data, size_t len, bool is_final) {
      // Resize buffer to the current sample size
      size_t currentSize = currentSampleSize();
      if (currentSize == 0) {
        LOGE("No sample size defined, cannot write data");
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
          if (box_pos >= box_size){
            LOGI("Reached end of box: %s write", is_final ? "final" : "not final");
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

    Vector<size_t>& getSampleSizes() { return sampleSizes; }
    Vector<size_t>& getChunkOffsets() { return chunkOffsets; }

    void setAacConfig(int profile, int srIdx, int chCfg) {
      aacProfile = profile;
      sampleRateIdx = srIdx;
      channelCfg = chCfg;
    }

   protected:
    Vector<size_t> sampleSizes;
    Vector<size_t> chunkOffsets;
    Codec codec = Codec::Unknown;
    FrameCallback callback = nullptr;
    void* ref = nullptr;
    size_t sampleIndex = 0;
    SingleBuffer<uint8_t> buffer;
    int aacProfile = 2, sampleRateIdx = 4, channelCfg = 2;
    size_t current_size = 0;  // current sample size
    size_t box_size = 0;      // maximum size of the current sample
    size_t box_pos = 0;

    void executeCallback(size_t size) {
      size_t frameSize = size;
      Frame frame;
      frame.codec = codec;
      frame.data = buffer.data();
      frame.size = frameSize;
      frame.timestamp = 0;  // TODO: timestamp
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

    void resize(size_t newSize) {
      if (buffer.size() < newSize) {
        buffer.resize(newSize);
      }
    }

    size_t currentSampleSize() {
      if (sampleSizes && sampleIndex < sampleSizes.size()) {
        return sampleSizes[sampleIndex];
      }
      return 0;
    }

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

  M4AAudioDemuxer() {
    parser.setReference(this);
    parser.setCallback(boxCallback);

    // incremental data callback
    parser.setDataCallback(boxDataCallback);
  }

  void setCallback(FrameCallback cb) {
    sampleExtractor.setReference(ref);
    sampleExtractor.setCallback(cb);
  }

  void begin() {
    codec = Codec::Unknown;
    alacMagicCookie.clear();
    resize(default_size);

    // When codec/sampleSizes/callback/ref change, update the extractor:
    parser.begin();
    sampleExtractor.begin();
  }

  void write(const uint8_t* data, size_t len) { parser.write(data, len); }

  int availableForWrite() { return parser.availableForWrite(); }

  Vector<uint8_t>& getAlacMagicCookie() { return alacMagicCookie; }

  void setReference(void* ref) { this->ref = ref; }

  void resize(int size) {
    default_size = size;
    if (buffer.size() < size) {
      buffer.resize(size);
    }
  }

 protected:
  MP4ParserIncremental parser;
  Codec codec = Codec::Unknown;
  Vector<uint8_t> alacMagicCookie;
  SingleBuffer<uint8_t> buffer;  // buffer to collect incremental data
  SampleExtractor sampleExtractor;
  void* ref = nullptr;
  size_t default_size = 2 * 1024;

  bool isRelevantBox(const char* type) {
    // Check if the box is relevant for audio demuxing
    return (StrView(type) == "stsd" || StrView(type) == "stsz" ||
            StrView(type) == "stco");
  }

  /// Just prints the box name and the number of bytes received
  static void boxCallback(MP4Parser::Box& box, void* ref) {
    M4AAudioDemuxer& self = *static_cast<M4AAudioDemuxer*>(ref);

    // mdat must not be buffered
    if (StrView(box.type) == "mdat") {
      LOGI("Box: %s, size: %u bytes", box.type, (unsigned)box.size);
      // self.sampleExtractor.setCodec(self.codec);
      self.sampleExtractor.setMaxSize(box.size);
      return;
    }

    bool is_relevant = self.isRelevantBox(box.type);
    if (is_relevant) {
      LOGI("Box: %s, size: %u bytes", box.type, (unsigned)box.size);
      if (box.data_size == 0) {
        // setup for increemental processing
        self.resize(box.size);
        self.buffer.clear();
      } else {
        // we have the complete box data
        self.processBox(box);
      }
    }
  }

  static void boxDataCallback(MP4Parser::Box& box, const uint8_t* data,
                              size_t len, bool is_final, void* ref) {
    M4AAudioDemuxer& self = *static_cast<M4AAudioDemuxer*>(ref);

    // mdat must not be buffered
    if (StrView(box.type) == "mdat") {
      LOGI("*Box: %s, size: %u bytes", box.type, (unsigned)len);
      // self.sampleExtractor.setCodec(self.codec);
      self.sampleExtractor.write(data, len, is_final);
      return;
    }

    // only process relevant boxes
    if (!self.isRelevantBox(box.type)) return;

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

  void processBox(MP4Parser::Box& box) {
    if (StrView(box.type) == "stsd") {
      onStsd(box);
    } else if (StrView(box.type) == "stsz") {
      onStsz(box);
    } else if (StrView(box.type) == "stco") {
      onStco(box);
    }
  }

  static uint32_t readU32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
  }

  void onStsd(const MP4Parser::Box& box) {
    LOGI("onStsd: %s, size: %zu bytes", box.type, box.data_size);
    const uint8_t* data = box.data;  // skip version/flags ?
    size_t size = box.data_size;
    if (size < 8) return;
    uint32_t entryCount = readU32(data + 4);
    size_t cursor = 8;
    for (uint32_t i = 0; i < entryCount; ++i) {
      if (cursor + 8 > size) break;
      uint32_t entrySize = readU32(data + cursor);
      const char* entryType = (const char*)(data + cursor + 4);
      if (entrySize < 36 || cursor + entrySize > size) break;
      size_t childrenStart = cursor + 8 + 28;
      size_t childrenEnd = cursor + entrySize;
      codec = Codec::Unknown;
      if (StrView(entryType) == "mp4a") {
        LOGI("-> AAC")
        codec = Codec::AAC;
        sampleExtractor.setCodec(codec);
        onStsdHandleMp4a(data, size, childrenStart, childrenEnd);
        break;
      } else if (StrView(entryType) == ".mp3") {
        LOGI("-> MP3")
        codec = Codec::MP3;
        sampleExtractor.setCodec(codec);
        break;
      } else if (StrView(entryType) == "alac") {
        LOGI("-> ALAC")
        codec = Codec::ALAC;
        sampleExtractor.setCodec(codec);
        onStsdHandleAlac(data, size, childrenStart, childrenEnd);
        break;
      }
      cursor += entrySize;
    }
  }

  void onStsdHandleMp4a(const uint8_t* data, size_t size, size_t childrenStart,
                        size_t childrenEnd) {
    int aacProfile = 2;     // Default: AAC LC
    int sampleRateIdx = 4;  // Default: 44100 Hz
    int channelCfg = 2;     // Default: Stereo

    // Look for 'esds' box inside children
    size_t childOffset = childrenStart;
    while (childOffset + 8 <= childrenEnd && childOffset + 8 <= size) {
      uint32_t childSize = readU32(data + childOffset);
      const char* childType = (const char*)(data + childOffset + 4);
      if (childSize < 8 || childOffset + childSize > size) break;
      if (StrView(childType) == "esds") {
        onStsdParseEsdsForAacConfig(data + childOffset + 8, childSize - 8,
                                    aacProfile, sampleRateIdx, channelCfg);
        break;
      }
      childOffset += childSize;
    }
    sampleExtractor.setAacConfig(aacProfile, sampleRateIdx, channelCfg);
  }

  void onStsdParseEsdsForAacConfig(const uint8_t* esds, size_t esdsLen,
                                   int& aacProfile, int& sampleRateIdx,
                                   int& channelCfg) {
    for (size_t j = 0; j + 2 < esdsLen; ++j) {
      if (esds[j] == 0x05) {  // 0x05 = AudioSpecificConfig tag
        // Next byte is length, then AudioSpecificConfig
        const uint8_t* asc = esds + j + 2;
        aacProfile = ((asc[0] >> 3) & 0x1F);  // 5 bits
        sampleRateIdx =
            ((asc[0] & 0x07) << 1) | ((asc[1] >> 7) & 0x01);  // 4 bits
        channelCfg = (asc[1] >> 3) & 0x0F;                    // 4 bits
        break;
      }
    }
  }

  void onStsdHandleAlac(const uint8_t* data, size_t size, size_t childrenStart,
                        size_t childrenEnd) {
    size_t childOffset = childrenStart;
    while (childOffset + 8 <= childrenEnd && childOffset + 8 <= size) {
      uint32_t childSize = readU32(data + childOffset);
      const char* childType = (const char*)(data + childOffset + 4);
      if (childSize < 8 || childOffset + childSize > size) break;
      if (StrView(childType) == "alac") {
        alacMagicCookie.resize(childSize - 8);
        std::memcpy(alacMagicCookie.data(), data + childOffset + 8,
                    childSize - 8);
        break;
      }
      childOffset += childSize;
    }
  }

  void onStsz(MP4Parser::Box& box) {
    LOGI("onStsz: %s, size: %zu bytes", box.type, box.data_size);
    // Parse stsz box and fill sampleSizes
    const uint8_t* data = box.data;  // skip version/flags
    size_t size = box.data_size;
    if (size < 12) return;
    uint32_t sampleSize = readU32(data + 4);
    uint32_t sampleCount = readU32(data + 8);
    sampleExtractor.begin();
    Vector<size_t>& sampleSizes = sampleExtractor.getSampleSizes();
    if (sampleSize == 0) {
      if (size < 12 + 4 * sampleCount) return;
      LOGI("-> Sample Sizes Count: %u", sampleCount);
      sampleSizes.resize(sampleCount);
      for (uint32_t i = 0; i < sampleCount; ++i) {
        sampleSizes[i] = readU32(data + 12 + i * 4);
      }
    } else {
      sampleSizes.assign(sampleCount, sampleSize);
    }
  }

  void onStco(MP4Parser::Box& box) {
    LOGI("onStco: %s, size: %zu bytes", box.type, box.data_size);
    // Parse stco box and fill chunkOffsets
    const uint8_t* data = box.data + 4;  // skip version/flags
    size_t size = box.data_size;
    if (size < 4) return;
    uint32_t entryCount = readU32(data);
    Vector<size_t>& chunkOffsets = sampleExtractor.getChunkOffsets();
    if (size < 4 + 4 * entryCount) return;
    chunkOffsets.resize(entryCount);
    LOGI("-> Chunk offsets count: %u", entryCount);
    for (uint32_t i = 0; i < entryCount; ++i) {
      chunkOffsets[i] = readU32(data + 4 + i * 4);
    }
  }
};

}  // namespace audio_tools