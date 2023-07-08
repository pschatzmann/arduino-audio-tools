#pragma once
#include "AudioLogger.h"

#define htonl(x)                                            \
  (((x) << 24 & 0xFF000000UL) | ((x) << 8 & 0x00FF0000UL) | \
   ((x) >> 8 & 0x0000FF00UL) | ((x) >> 24 & 0x000000FFUL))
#define ntohl(x) htonl(x)
#define htons(x) (((uint16_t)(x)&0xff00) >> 8) | (((uint16_t)(x)&0X00FF) << 8)
#define ntohs(x) htons(x)

namespace audio_tools {

/**
 * @brief Represents a single MPEG4 atom
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct MP4Atom {
  MP4Atom() = default;
  MP4Atom(const char *atom) { strncpy(this->atom, atom, 4); }
  // start pos in data stream
  size_t start_pos;

  /// size w/o size filed and atom
  uint32_t size = 0;
  /// 4 digit atom name
  char atom[5] = {0};
  /// true if atom is header w/o data content
  bool is_header_atom;
  // data
  const uint8_t *data = nullptr;
  // length of the data
  uint32_t data_size = 0;

  void setHeader(uint8_t *data, int len) {
    uint32_t *p_size = (uint32_t *)data;
    size = ntohl(*p_size) - 8;
    memcpy(atom, data + 4, 4);
    // it is a header atom when the next atom just follows it
    is_header_atom = isalpha(data[12]) && isalpha(data[13]) &&
                     isalpha(data[14]) && isalpha(data[15]);
    LOGI("%s %d - %s", atom, size, is_header_atom ? "header" : "atom");
  }

  bool is(const char *atom) { return strncmp(this->atom, atom, 4) == 0; }

  void setData(const uint8_t *data, int len) {
    this->data = data;
    data_size = len;
    // assert(size == len);
  }

  void clear() {
    size = 0;
    memset(atom, 0, 5);
    data = nullptr;
    data_size = 0;
  }

  bool isHeader() { return is_header_atom; }
  operator bool() { return strlen(atom) == 4; }

  uint16_t read16(int pos) {
    if (size < pos) return 0;
    uint16_t *ptr = (uint16_t *)data + pos;
    return ntohs(*ptr);
  }
  uint32_t read32(int pos) {
    if (size < pos) return 0;
    uint32_t *ptr = (uint32_t *)data + pos;
    return ntohl(*ptr);
  }
};

/***
 * @brief Buffer which is used for parsing the mpeg4 data
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MP4ParseBuffer {
 public:
  MP4ParseBuffer() = default;
  // provides the data
  size_t write(const uint8_t *data, size_t length) {
    // initialize buffer size
    if (buffer.size() == 0) buffer.resize(length);
    return buffer.writeArray(data, length);
  }

  /// returns the parsed atoms
  MP4Atom parse() {
    // determine atom length from header of buffer
    MP4Atom result;
    while (true) {
      if (buffer.available() < 64) break;

      uint8_t header[64];
      buffer.peekArray(header, 64);
      result.setHeader(header, 64);
      int16_t len = peekLength();

      if (result.is_header_atom) {
        // consume data for header atom
        buffer.readArray(header, 32);
      } else {
        if (len <= buffer.available()) {
          // not enough data
          break;
        } else {
          buffer.readArray(header, 32);
          uint8_t data[len - 32];
          buffer.readArray(data, len - 32);
          result.setData(data, len - 32);
        }
      }
      return result;
    }
  }

  int available() { return buffer.available(); }

  size_t readArray(uint8_t *data, size_t len) {
    return buffer.readArray(data, len);
  }

 protected:
  RingBuffer<uint8_t> buffer{0};

  // determines the actual length attribute value from the atom at the head of
  // the buffer
  int16_t peekLength() {
    int16_t len;
    buffer.peekArray((uint8_t *)&len, 4);
    len = ntohl(len);
    return len;
  }
};

/**
 * @brief Minimum flexible parser for MPEG4 data (which is based on the
 * Quicktime format). Small atoms will be make available via a callback method.
 * The big (audio) content is written to the Print object which was specified in
 * the constructor.
 * @ingroup codecs
 * @ingroup decoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ContainerMP4 : public AudioStream {
 public:
  ContainerMP4(Print &out, const char *streamAtom = "mdat") {
    p_print = &out;
    stream_atom = streamAtom;
  }

  bool begin() { current_pos = 0; }

  /// writes the next atom
  size_t write(const uint8_t *data, size_t length) override {
    // direct output to stream
    if (stream_out_open > 0) {
      size_t len = min(stream_out_open, length);
      size_t result = len;
      MP4Atom atom{stream_atom};
      atom.size = len;
      atom.data = data;
      if (callback != nullptr) callback(atom, *this);
      current_pos += len;
      stream_out_open -= result;
      return result;
    }

    // parse data and provide info via callback
    size_t result = buffer.write(data, length);
    MP4Atom atom = buffer.parse();
    while (atom) {
      atom.start_pos = current_pos;
      if (callback != nullptr) callback(atom, *this);
      current_pos += atom.size + 8;
      atom = buffer.parse();
    }

    // write data of mdat to print
    if (atom.is(stream_atom)) {
      setStreamOutputSize(atom.size);
      size_t len = buffer.available();
      uint8_t tmp[len];
      buffer.readArray(tmp, len);

      MP4Atom atom{stream_atom};
      atom.start_pos = current_pos;
      atom.size = len;
      atom.data = tmp;
      if (callback != nullptr) callback(atom, *this);
      current_pos += len;

      stream_out_open -= result;
    }

    return result;
  }

  /// Defines the callback that is executed on each atom
  void setCallback(void (*cb)(MP4Atom atom, ContainerMP4 &container)) {
    callback = cb;
  }

  /// output of mdat to p_print;
  size_t print(const uint8_t *data, size_t len) {
    return p_print == nullptr ? 0 : p_print->write(data, len);
  }

  /// Provides the content atom which will be written incrementally
  const char *streamAtom() { return stream_atom; }

 protected:
  MP4ParseBuffer buffer;
  int stream_out_open = 0;
  Print *p_print = nullptr;
  const char *stream_atom;
  size_t current_pos = 0;
  void (*callback)(MP4Atom atom, ContainerMP4 &container) = default_callback;

  void setStreamOutputSize(int size) { stream_out_open = size; }

  static void default_callback(MP4Atom atom, ContainerMP4 &container) {
    // parse ftyp to determine the subtype
    if (atom.is("ftyp")) {
      char subtype[5];
      strncpy(subtype, (char *)atom.data + 8, 4);
      LOGI("subtype: %s", subtype);
    }

    // parse stsd -> audio info
    if (atom.is("stsd")) {
      AudioInfo info;
      info.channels = atom.read16(8 + 0x20);
      info.bits_per_sample = atom.read16(8 + 0x22);  // not used
      info.sample_rate = atom.read32(8 + 0x26);
      info.logInfo();
      container.setAudioInfo(info);
    }

    /// output of mdat to p_print;
    if (atom.is(container.streamAtom())) {
      container.print(atom.data, atom.data_size);
    }
  }
};

}  // namespace audio_tools