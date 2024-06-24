#pragma once
#include "AudioCodecs/AudioCodecsBase.h"
#include "AudioCodecs/CodecAACHelix.h"
#include "AudioBasic/Net.h"
#include "AudioLogger.h"

namespace audio_tools {

class ContainerMP4;

/**
 * @brief Represents a single MPEG4 atom
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct MP4Atom {
  friend class ContainerMP4;
  MP4Atom(ContainerMP4 *container) { this->container = container; };
  MP4Atom(ContainerMP4 *container, const char *atom) {
    strncpy(this->atom, atom, 4);
    this->container = container;
  }
  // start pos in data stream
  int start_pos = 0;
  /// size w/o size and atom fields
  int total_size = 0;
  int data_size = 0;
  /// 4 digit atom name
  char atom[5] = {0};
  /// true if atom is header w/o data content
  bool is_header_atom;
  // data
  const uint8_t *data = nullptr;
  // length of the data
  // uint32_t data_size = 0;
  // pointer to parent container
  ContainerMP4 *container = nullptr;
  /// data is provided in
  bool is_stream = false;

  void setHeader(uint8_t *data, int total);

  /// Compares the atom name
  bool is(const char *atom) { return strncmp(this->atom, atom, 4) == 0; }

  // /// Incomplete atom data, we send the data in individual chunks
  // bool isStream();

  /// Atom which is sent to print output
  bool isStreamAtom();

  operator bool() {
    return isalpha(atom[0]) && isalpha(atom[1]) && isalpha(atom[2]) &&
           isalpha(atom[3]) && total_size >= 0;
  }

  /// Updates the data and size field
  void setData(const uint8_t *data, int len_data) {
    this->data = data;
    this->data_size = len_data;
    // assert(size == len);
  }

  /// Clears the atom
  void clear() {
    total_size = 0;
    data_size = 0;
    memset(atom, 0, 5);
    data = nullptr;
  }

  /// Returns true if the atom is a header atom
  bool isHeader() { return is_header_atom; }

  uint16_t read16(int pos) {
    if (data_size < pos) return 0;
    uint16_t *ptr = (uint16_t *)(data + pos);
    return ntohs(*ptr);
  }
  uint32_t read32(int pos) {
    if (data_size < pos) return 0;
    uint32_t *ptr = (uint32_t *)(data + pos);
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
  MP4ParseBuffer(ContainerMP4 *container) { this->container = container; };
  // provides the data
  size_t write(const uint8_t *data, size_t len) {
    // initialize buffer size
    if (buffer.size() == 0) buffer.resize(len);
    return buffer.writeArray(data, len);
  }

  /// returns the parsed atoms
  MP4Atom parse();

  int available() { return buffer.available(); }

  size_t readArray(uint8_t *data, size_t len) {
    return buffer.readArray(data, len);
  }

 protected:
  RingBuffer<uint8_t> buffer{1024};
  ContainerMP4 *container = nullptr;
};

/**
 * @brief Minimum flexible parser for MPEG4 data (which is based on the
 * Quicktime format). Small atoms will be make available via a callback method.
 * The big (audio) content is written to the Print object which was specified in
 * the constructor. Depends on https://github.com/pschatzmann/arduino-libhelix!
 * @ingroup codecs
 * @ingroup decoder
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ContainerMP4 : public ContainerDecoder {
  friend class MP4ParseBuffer;

 public:
  ContainerMP4(const char *streamAtom = "mdat") {
    stream_atom = streamAtom;
  }

  ContainerMP4(AudioDecoder &decoder, const char *streamAtom = "mdat") {
    stream_atom = streamAtom;
    p_decoder = &decoder;
  }

  ContainerMP4(AudioDecoder *decoder, const char *streamAtom = "mdat") {
    stream_atom = streamAtom;
    p_decoder = decoder;
  }

  /// starts the processing
  bool begin() override {
    current_pos = 0;
    assert(p_print!=nullptr);
    p_decoder->setOutput(*p_print);
    bool rc = p_decoder->begin();
    is_active = true;
    return rc;
  }

  /// ends the processing
  void end() override { 
    p_decoder->end(); 
    is_active = false;
  }

  operator bool() override { return is_active; }

  /// writes the data to be parsed into atoms
  size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    // initialize the max_size with copy length
    if (max_size == 0) setMaxSize(len);

    // direct output to stream
    if (stream_out_open > 0) {
      int len = min(stream_out_open, (int)len);
      int result = len;
      MP4Atom atom{this, stream_atom};
      atom.total_size = len;
      atom.data_size = len;
      atom.data = data;
      atom.start_pos = current_pos;

      if (data_callback != nullptr) data_callback(atom, *this);
      current_pos += len;
      stream_out_open -= result;
      return result;
    }

    // parse data and provide info via callback
    size_t result = buffer.write(data, len);
    MP4Atom atom = buffer.parse();
    while (atom && !atom.is_stream) {
      // atom.start_pos = current_pos;
      // current_pos += atom.total_size;
      atom = buffer.parse();
    }

    return result;
  }

  /// Defines the callback that is executed on each atom
  void setDataCallback(void (*cb)(MP4Atom &atom, ContainerMP4 &container)) {
    data_callback = cb;
  }

  /// Defines the callback which is used to determine if an atom is a header atom
  void setIsHeaderCallback(bool (*cb)(MP4Atom *atom, const uint8_t *data)) {
    is_header_callback = cb;
  }

  /// Provides the content atom name which will be written incrementally
  const char *streamAtom() { return stream_atom; }

  /// Checks if the indicated atom is a header atom: you can define your custom method with setIsHeaderCallback()
  bool isHeader(MP4Atom *atom, const uint8_t *data) {
    return is_header_callback(atom, data);
  }

  /// Defines the maximum size that we can submit to the decoder
  void setMaxSize(int size) { max_size = size; }

  /// Provides the maximum size 
  int maxSize() { return max_size; }

 protected:
  int max_size;
  MP4ParseBuffer buffer{this};
  int stream_out_open = 0;
  bool is_sound = false;
  bool is_active = false;
  AACDecoderHelix aac_decoder;
  AudioDecoder* p_decoder = &aac_decoder;
  const char *stream_atom;
  int current_pos = 0;
  const char *current_atom = nullptr;
  void (*data_callback)(MP4Atom &atom,
                        ContainerMP4 &container) = default_data_callback;
  bool (*is_header_callback)(MP4Atom *atom,
                             const uint8_t *data) = default_is_header_callback;

  /// output of audio mdat to helix decoder;
  size_t decode(const uint8_t *data, size_t len) {
    return p_decoder->write(data, len);
  }

  /// Defines the size of open data that will be written directly w/o parsing
  void setStreamOutputSize(int size) { stream_out_open = size; }

  /// Default logic to determine if a atom is a header
  static bool default_is_header_callback(MP4Atom *atom, const uint8_t *data) {
    // it is a header atom when the next atom just follows it
    bool is_header_atom = isalpha(data[12]) && isalpha(data[13]) &&
                          isalpha(data[14]) && isalpha(data[15]) &&
                          atom->data_size > 0;
    return is_header_atom;
  }

  /// Default logic to process an atom
  static void default_data_callback(MP4Atom &atom, ContainerMP4 &container) {
    // char msg[80];
    // snprintf(msg, 80, "%s %d-%d %d", atom.atom, (int)atom.start_pos,
    // atom.start_pos+atom.total_size,  atom.total_size);
    LOGI("%s: 0x%06x-0x%06x %d %s", atom.atom, (int)atom.start_pos,
         (int)atom.start_pos + atom.total_size, atom.total_size, atom.is_header_atom?" *":"");
    if (atom.total_size > 1024) {
      TRACED();
    }
    // parse ftyp to determine the subtype
    if (atom.is("ftyp") && atom.data != nullptr) {
      char subtype[5];
      memcpy(subtype, atom.data, 4);
      subtype[4] = 0;
      LOGI("    subtype: %s", subtype);
    }

    // parse hdlr to determine if audio
    if (atom.is("hdlr") && atom.data != nullptr) {
      const uint8_t *sound = atom.data + 8;
      container.is_sound = memcmp("soun", sound, 4) == 0;
      LOGI("    is_sound: %s", container.is_sound ? "true" : "false");
    }

    // parse stsd -> audio info
    if (atom.is("stsd")) {
      AudioInfo info;
      info.channels = atom.read16(0x20);
      info.bits_per_sample = atom.read16(0x22);  // not used
      info.sample_rate = atom.read32(0x26);
      info.logInfo();
      container.setAudioInfo(info);
      // init raw output
      container.p_decoder->setAudioInfo(info);
    }

    /// output of mdat to decoder;
    if (atom.isStreamAtom()) { // 
      if (container.is_sound){
        int pos = 0;
        int open = atom.data_size;
        while (open>0){
          int processed = container.decode(atom.data+pos, open);
          open -=processed;
          pos +=processed;
        }

      } else {
        LOGD("%s: %d bytes ignored", container.stream_atom, atom.data_size);
      }
    }
  }
};

//-------------------------------------------------------------------------------------------------------------------
// methods
//--------------------------------------------------------------------------------------------------------------------

void MP4Atom::setHeader(uint8_t *data, int len) {
  uint32_t *p_size = (uint32_t *)data;
  total_size = ntohl(*p_size);
  data_size = total_size - 8;
  memcpy(atom, data + 4, 4);
  if (total_size==1){

  }

  is_header_atom = container->isHeader(this, data);
}

bool MP4Atom::isStreamAtom() {
  const char *name = container->streamAtom();
  return is(name);
}

MP4Atom MP4ParseBuffer::parse() {
  TRACED();
  // determine atom length from header of buffer
  MP4Atom result{container};
  result.start_pos = container->current_pos;

  // read potentially 2 headers
  uint8_t header[16];
  buffer.peekArray(header, 16);
  result.setHeader(header, 16);
  result.is_stream = false;
  container->current_atom = result.atom;

  // abort on invalid atom
  if (!result) {
    LOGE("Invalid atom");
    return result;
  }

  // make sure that we fill the buffer to it's max limit
  if (result.data_size > buffer.available() &&
      buffer.available() != container->maxSize()) {
    result.clear();
    return result;
  }

  // temp buffer for data
  uint8_t data[min(result.data_size, buffer.available())];

  if (result.is_header_atom) {
    // consume data for header atom
    buffer.readArray(header, 8);
    container->current_pos += 8;
  } else {
    if (result.total_size > buffer.available()) {
      // not enough data use streaming
      LOGI("total %s: 0x%06x-0x%06x - %d", container->current_atom,
           container->current_pos, container->current_pos + result.total_size,
           result.data_size);
      int avail = buffer.available() - 8;
      container->setStreamOutputSize(result.data_size - avail);

      buffer.readArray(header, 8);
      buffer.readArray(data, avail);
      result.setData(data, avail);
      result.total_size = avail;
      result.data_size = avail - 8;
      result.is_stream = true;
      assert(buffer.available()==0);
      container->current_pos += result.total_size;
    } else {
      buffer.readArray(header, 8);
      buffer.readArray(data, result.data_size);
      result.setData(data, result.data_size);
      container->current_pos += result.total_size;
    }
  }

  // callback for data
  if (result && container->data_callback != nullptr)
    container->data_callback(result, *container);

  return result;
}

}  // namespace audio_tools