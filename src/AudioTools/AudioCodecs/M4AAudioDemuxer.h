#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

/***
 * @brief M4AAudioDemuxer is a class that demuxes audio frames from M4A/MP4
 * files. It supports AAC, ALAC, and MP3 codecs. It provides a callback
 * mechanism to process demuxed audio frames.
 * @ingroup codecs
 * @author Phil Schatzmann
 */
class M4AAudioDemuxer {
 public:
  enum class Codec { AAC, ALAC, MP3, Unknown };

  struct Frame {
    Codec codec;
    const char* mime = "";
    const uint8_t* data;
    size_t size;
    uint64_t timestamp;
  };

  using FrameCallback = std::function<void(const Frame&, void* ref)>;

  M4AAudioDemuxer(FrameCallback cb) : callback(cb) { begin(); }

  void begin() {
    buffer.clear();
    mdatOffset = 0;
    mdatSize = 0;
    mdatFileOffset = 0;
    mdatDataOffset = 0;
    foundMoov = false;
    sampleIndex = 0;
    sampleSizes.clear();
    sampleOffsets.clear();
    chunkOffsets.clear();
    codec = Codec::Unknown;
    alacMagicCookie.clear();
  }

  void write(const uint8_t* data, size_t len) {
    buffer.writeArray(data, len);
    parse();
  }

  int availableForWrite() {
    return buffer.availableForWrite();
  }

  /// Returns the aloac magic cookie if available
  Vector<uint8_t>& getAlacMagicCookie() { return alacMagicCookie; }

  /// Provides an optional reference e.g. to the parent decoder
  void setReference(void* ref) { this->ref = ref; }

 private:
  FrameCallback callback;
  SingleBuffer<uint8_t> buffer;
  size_t mdatOffset = 0;
  size_t mdatSize = 0;
  size_t mdatFileOffset = 0;
  size_t mdatDataOffset = 0;
  bool foundMoov = false;
  size_t sampleIndex = 0;
  Vector<size_t> sampleSizes;
  Vector<size_t> sampleOffsets;
  Vector<size_t> chunkOffsets;
  Codec codec = Codec::Unknown;
  Vector<uint8_t> alacMagicCookie;
  void* ref = nullptr;  // Placeholder for any reference if needed

  static uint32_t readU32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
  }

  static uint64_t readU64(const uint8_t* p) {
    return ((uint64_t)readU32(p) << 32) | readU32(p + 4);
  }

  static bool boxNameEquals(const uint8_t* box, const char* name) {
    return std::memcmp(box, name, 4) == 0;
  }

  void parse() {
    size_t pos = 0;
    while (pos + 8 <= buffer.size()) {
      uint32_t boxSize = readU32(&buffer.data()[pos]);
      const uint8_t* boxType = &buffer.data()[pos + 4];

      if (boxSize == 1 && pos + 16 <= buffer.size()) {
        boxSize = (uint32_t)readU64(&buffer.data()[pos + 8]);
        pos += 8;
      }

      if (boxSize < 8 || pos + boxSize > buffer.size()) break;

      if (boxNameEquals(boxType, "ftyp")) {
        pos += boxSize;
      } else if (boxNameEquals(boxType, "moov")) {
        parseMoov(pos + 8, boxSize - 8);
        foundMoov = true;
        pos += boxSize;
      } else if (boxNameEquals(boxType, "mdat")) {
        mdatOffset = pos + 8;
        mdatSize = boxSize - 8;
        mdatFileOffset = pos;
        mdatDataOffset = 0;
        extractSamples();
        pos += boxSize;
      } else {
        pos += boxSize;
      }
    }

    if (pos > 0) {
      buffer.clearArray(pos);
      if (mdatOffset >= pos)
        mdatOffset -= pos;
      else
        mdatOffset = 0;

      if (mdatDataOffset >= pos)
        mdatDataOffset -= pos;
      else
        mdatDataOffset = 0;

      for (size_t& off : sampleOffsets) {
        if (off >= pos)
          off -= pos;
        else
          off = 0;
      }
    }
  }

  void parseMoov(size_t offset, size_t size) {
    size_t end = offset + size;
    while (offset + 8 <= end && offset + 8 <= buffer.size()) {
      uint32_t boxSize = readU32(&buffer.data()[offset]);
      const uint8_t* boxType = &buffer.data()[offset + 4];

      if (boxSize < 8 || offset + boxSize > buffer.size()) break;

      if (boxNameEquals(boxType, "trak")) {
        parseTrak(offset + 8, boxSize - 8);
      }

      offset += boxSize;
    }
  }

  void parseTrak(size_t offset, size_t size) {
    size_t end = offset + size;
    while (offset + 8 <= end && offset + 8 <= buffer.size()) {
      uint32_t boxSize = readU32(&buffer.data()[offset]);
      const uint8_t* boxType = &buffer.data()[offset + 4];

      if (boxSize < 8 || offset + boxSize > buffer.size()) break;

      if (boxNameEquals(boxType, "mdia")) {
        parseMdia(offset + 8, boxSize - 8);
      }

      offset += boxSize;
    }
  }

  void parseMdia(size_t offset, size_t size) {
    size_t end = offset + size;
    while (offset + 8 <= end && offset + 8 <= buffer.size()) {
      uint32_t boxSize = readU32(&buffer.data()[offset]);
      const uint8_t* boxType = &buffer.data()[offset + 4];

      if (boxSize < 8 || offset + boxSize > buffer.size()) break;

      if (boxNameEquals(boxType, "minf")) {
        parseMinf(offset + 8, boxSize - 8);
      }

      offset += boxSize;
    }
  }

  void parseMinf(size_t offset, size_t size) {
    size_t end = offset + size;
    while (offset + 8 <= end && offset + 8 <= buffer.size()) {
      uint32_t boxSize = readU32(&buffer.data()[offset]);
      const uint8_t* boxType = &buffer.data()[offset + 4];

      if (boxSize < 8 || offset + boxSize > buffer.size()) break;

      if (boxNameEquals(boxType, "stbl")) {
        parseStbl(offset + 8, boxSize - 8);
      }

      offset += boxSize;
    }
  }

  void parseStbl(size_t offset, size_t size) {
    size_t end = offset + size;
    while (offset + 8 <= end && offset + 8 <= buffer.size()) {
      uint32_t boxSize = readU32(&buffer.data()[offset]);
      const uint8_t* boxType = &buffer.data()[offset + 4];

      if (boxSize < 8 || offset + boxSize > buffer.size()) break;

      if (boxNameEquals(boxType, "stsd")) {
        parseStsd(offset + 8, boxSize - 8);
      } else if (boxNameEquals(boxType, "stsz")) {
        parseStsz(offset + 8, boxSize - 8);
      } else if (boxNameEquals(boxType, "stco")) {
        parseStco(offset + 8, boxSize - 8);
      }

      offset += boxSize;
    }
  }

  void parseStsd(size_t offset, size_t size) {
    if (offset + 8 > buffer.size()) return;

    const uint8_t* ptr = &buffer.data()[offset];

    // version (1 byte) + flags (3 bytes) = 4 bytes, then entry_count (4 bytes)
    if (offset + 8 > buffer.size()) return;
    uint32_t entryCount = readU32(ptr + 4);

    size_t cursor = offset + 8;
    for (uint32_t i = 0; i < entryCount; ++i) {
      if (cursor + 8 > buffer.size()) break;

      uint32_t entrySize = readU32(&buffer.data()[cursor]);
      const uint8_t* entryType = &buffer.data()[cursor + 4];

      if (entrySize < 36 || cursor + entrySize > buffer.size()) break;
      // 36 = minimal size: 8 bytes header + 28 bytes AudioSampleEntry fields

      codec = Codec::Unknown;

      // The AudioSampleEntry box structure:
      // size(4), type(4), reserved(6), data_reference_index(2), etc.
      // The first 8 bytes: size + type
      // Next 28 bytes fixed AudioSampleEntry fields before children boxes
      size_t childrenStart = cursor + 8 + 28;
      size_t childrenEnd = cursor + entrySize;

      if (boxNameEquals(entryType, "mp4a")) {
        codec = Codec::AAC;
        // Look for 'esds' box inside children
        size_t childOffset = childrenStart;
        while (childOffset + 8 <= childrenEnd &&
               childOffset + 8 <= buffer.size()) {
          uint32_t childSize = readU32(&buffer.data()[childOffset]);
          const uint8_t* childType = &buffer.data()[childOffset + 4];

          if (childSize < 8 || childOffset + childSize > buffer.size()) break;

          if (boxNameEquals(childType, "esds")) {
            // We can add ESDS parsing here if needed
            break;
          }
          childOffset += childSize;
        }

      } else if (boxNameEquals(entryType, ".mp3")) {
        codec = Codec::MP3;

      } else if (boxNameEquals(entryType, "alac")) {
        codec = Codec::ALAC;

        // Look for 'alac' box inside children
        size_t childOffset = childrenStart;
        while (childOffset + 8 <= childrenEnd &&
               childOffset + 8 <= buffer.size()) {
          uint32_t childSize = readU32(&buffer.data()[childOffset]);
          const uint8_t* childType = &buffer.data()[childOffset + 4];

          if (childSize < 8 || childOffset + childSize > buffer.size()) break;

          if (boxNameEquals(childType, "alac")) {
            //alacMagicCookie.assign(buffer.data() + childOffset + 8,
            //                       buffer.data() + childOffset + childSize);
            alacMagicCookie.resize(childSize - 8);
            std::memcpy(alacMagicCookie.data(), &buffer.data()[childOffset + 8],
                        childSize - 8);
            break;
          }
          childOffset += childSize;
        }
      }

      cursor += entrySize;
    }
  }

  void parseStsz(size_t offset, size_t size) {
    if (offset + 12 > buffer.size()) return;

    const uint8_t* ptr = &buffer.data()[offset];
    uint32_t sampleSize = readU32(ptr);
    uint32_t sampleCount = readU32(ptr + 4);

    sampleSizes.clear();

    if (sampleSize == 0) {
      // Sizes are in the table
      if (offset + 12 + 4 * sampleCount > buffer.size()) return;
      for (uint32_t i = 0; i < sampleCount; ++i) {
        sampleSizes.push_back(readU32(&buffer.data()[offset + 12 + i * 4]));
      }
    } else {
      // All samples have the same size
      sampleSizes.assign(sampleCount, sampleSize);
    }
  }

  void parseStco(size_t offset, size_t size) {
    if (offset + 4 > buffer.size()) return;

    const uint8_t* ptr = &buffer.data()[offset];
    uint32_t entryCount = readU32(ptr);

    chunkOffsets.clear();
    if (offset + 4 + 4 * entryCount > buffer.size()) return;

    for (uint32_t i = 0; i < entryCount; ++i) {
      chunkOffsets.push_back(readU32(&buffer.data()[offset + 4 + i * 4]));
    }
  }

  void extractSamples() {
    if (codec == Codec::Unknown) return;
    if (sampleSizes.empty() || chunkOffsets.empty()) return;

    // For simplicity, assume samples and chunks line up 1:1
    size_t offsetInMdat = 0;
    for (size_t i = 0; i < sampleSizes.size(); ++i) {
      size_t size = sampleSizes[i];
      if (mdatOffset + offsetInMdat + size > buffer.size()) break;

      Frame frame;
      frame.codec = codec;
      frame.data = &buffer.data()[mdatOffset + offsetInMdat];
      frame.size = size;
      frame.timestamp = 0;  // TODO: timestamp parsing
      switch (codec) {
        case Codec::AAC:
          frame.mime = "audio/aac";
          break;
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

      callback(frame, ref);

      offsetInMdat += size;
    }
  }
};

}  // namespace audio_tools
