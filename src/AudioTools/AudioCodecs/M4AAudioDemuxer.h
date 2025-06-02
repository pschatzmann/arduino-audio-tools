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
    const char* mime = "";
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
    }

    void setCodec(M4AAudioDemuxer::Codec c) { codec = c; }

    void setCallback(FrameCallback cb, void* r) {
      callback = cb;
      ref = r;
    }

    void write(const uint8_t* data, size_t len, bool is_final) {
      // Resize buffer to the current sample size
      size_t currentSize = currentSampleSize();
      resize(currentSize);

      /// fill buffer up to the current sample size
      for (int j = 0; j < len; j++) {
        buffer.write(data[j]);
        if (buffer.available() == currentSize) {
          executeCallback(currentSize);
          buffer.clear();
          ++sampleIndex;
          currentSize = currentSampleSize();
          resize(currentSize);
        }
      }
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

    void executeCallback(size_t size) {
      size_t frameSize = size;
      Frame frame;
      frame.codec = codec;
      frame.data = buffer.data();
      frame.size = frameSize;
      frame.timestamp = 0;  // TODO: timestamp
      switch (codec) {
        case Codec::AAC: {
          uint8_t adts[7];
          writeAdtsHeader(adts, aacProfile, sampleRateIdx, channelCfg,
                          frameSize);
          uint8_t out[frameSize + 7];
          memcpy(out, adts, 7);
          memcpy(out + 7, buffer.data(), frameSize);
          frame.data = out;
          frame.size = sizeof(out);
          frame.mime = "audio/aac";
          callback(frame, ref);
          return;
        }
        case Codec::ALAC:
          frame.mime = "audio/alac";
          break;
        case Codec::MP3:
          frame.mime = "audio/mpeg";
          break;
        default:
          frame.mime = "unknown";
          break;
      }
      if (callback) callback(frame, ref);
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

  M4AAudioDemuxer(FrameCallback cb) : callback(cb) {
    parser.setReference(this);
    // Generic callback which just provides the data in the buffer
    parser.setCallback(boxCallback);

    // incremental data callback
    parser.setDataCallback(boxDataCallback);

    // Add more as needed...
    parser.begin();
  }

  void begin() {
    parser.begin();
    codec = Codec::Unknown;
    alacMagicCookie.clear();

    // When codec/sampleSizes/callback/ref change, update the extractor:
    sampleExtractor.begin();
    sampleExtractor.setCallback(callback, ref);
  }

  void write(const uint8_t* data, size_t len) { parser.write(data, len); }

  int availableForWrite() { return parser.availableForWrite(); }

  Vector<uint8_t>& getAlacMagicCookie() { return alacMagicCookie; }
  void setReference(void* ref) { parser.setReference(ref); }

 private:
  FrameCallback callback;
  MP4ParserIncremental parser;
  Codec codec = Codec::Unknown;
  Vector<uint8_t> alacMagicCookie;
  SingleBuffer<uint8_t> buffer;  // buffer to collect incremental data
  SampleExtractor sampleExtractor;
  void* ref = nullptr;

  bool isRelevantBox(const char* type) {
    // Check if the box is relevant for audio demuxing
    return (StrView(type) == "stsd" || StrView(type) == "stsz" ||
            StrView(type) == "stco");
  }
  /// Just prints the box name and the number of bytes received
  static void boxCallback(MP4Parser::Box& box, void* ref) {
    M4AAudioDemuxer& self = *static_cast<M4AAudioDemuxer*>(ref);
    if (self.isRelevantBox(box.type)) {
      self.buffer.resize(box.size);
      self.buffer.clear();
      if (box.data_size > 0) self.buffer.writeArray(box.data, box.data_size);
    }
  }

  static void boxDataCallback(MP4Parser::Box& box, const uint8_t* data,
                              size_t len, bool is_final, void* ref) {
    M4AAudioDemuxer& self = *static_cast<M4AAudioDemuxer*>(ref);

    // mdat must not be buffered
    if (StrView(box.type) == "mdat") {
      self.sampleExtractor.setCodec(self.codec);
      self.sampleExtractor.write(data, len, is_final);
      return;
    }

    // only process relevant boxes
    if (!self.isRelevantBox(box.type) == false) return;

    // others fill buffer incrementally
    if (len > 0) {
      self.buffer.writeArray(data, len);
    }

    // on last chunk, call the specific box handler
    if (is_final) {
      MP4Parser::Box complete_box = box;
      complete_box.data = self.buffer.data();
      complete_box.data_size = self.buffer.size();
      if (StrView(box.type) == "stsd") {
        self.onStsd(complete_box);
      } else if (StrView(box.type) == "stsz") {
        self.onStsz(complete_box);
      } else if (StrView(box.type) == "stco") {
        self.onStco(complete_box);
      }
    }
  }

  static uint32_t readU32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
  }

  void onStsd(const MP4Parser::Box& box) {
    const uint8_t* data = box.data;
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
        codec = Codec::AAC;
        onStsdHandleMp4a(data, size, childrenStart, childrenEnd);
      } else if (StrView(entryType) == ".mp3") {
        codec = Codec::MP3;
      } else if (StrView(entryType) == "alac") {
        codec = Codec::ALAC;
        onStsdHandleAlac(data, size, childrenStart, childrenEnd);
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
    // Parse stsz box and fill sampleSizes
    const uint8_t* data = box.data;
    size_t size = box.data_size;
    if (size < 12) return;
    uint32_t sampleSize = readU32(data);
    uint32_t sampleCount = readU32(data + 4);
    sampleExtractor.begin();
    Vector<size_t>& sampleSizes = sampleExtractor.getSampleSizes();
    if (sampleSize == 0) {
      if (size < 12 + 4 * sampleCount) return;
      for (uint32_t i = 0; i < sampleCount; ++i) {
        sampleSizes.push_back(readU32(data + 12 + i * 4));
      }
    } else {
      sampleSizes.assign(sampleCount, sampleSize);
    }
  }

  void onStco(MP4Parser::Box& box) {
    // Parse stco box and fill chunkOffsets
    const uint8_t* data = box.data;
    size_t size = box.data_size;
    if (size < 4) return;
    uint32_t entryCount = readU32(data);
    Vector<size_t>& chunkOffsets =
        sampleExtractor.getChunkOffsets();
    chunkOffsets.clear();
    if (size < 4 + 4 * entryCount) return;
    for (uint32_t i = 0; i < entryCount; ++i) {
      chunkOffsets.push_back(readU32(data + 4 + i * 4));
    }
  }
};

}  // namespace audio_tools