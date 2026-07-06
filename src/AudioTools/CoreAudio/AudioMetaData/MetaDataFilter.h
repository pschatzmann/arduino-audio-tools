#pragma once
#include "AudioLogger.h"
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

/**
 * @brief Class which filters out ID3v1 and ID3v2 Metadata and provides only the
 * audio data to the decoder
 * @ingroup metadata
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MetaDataFilter : public AudioOutput {
 public:
  /// Default Constructor
  MetaDataFilter() = default;

  /// Constructor which assigns the decoder
  MetaDataFilter(Print &out) { setOutput(out); }
  /// Constructor which assigns the decoder
  MetaDataFilter(AudioWriter &out) { setOutput(out); }

  /// Defines the decoder to which we write the data
  void setOutput(Print &out) { p_out = &out; }
  /// Defines the decoder to which we write the data
  void setOutput(AudioWriter &out) { p_writer = &out; }

  /// (Re)starts the processing
  bool begin() override {
    TRACED();
    total_pos = 0;
    id3v2_skip_remaining = 0;
    tail.reset();
    out_buf.reset();
    if (p_writer) p_writer->begin();
    return true;
  }

  void end() override {
    // the last bytes are still held back in case they are a trailing
    // ID3v1/TAG+ tag: decide now and flush whatever is real audio
    flushTail();
    if (p_writer) p_writer->end();
  }

  /// Writes the data to the decoder
  size_t write(const uint8_t *data, size_t len) override {
    LOGI("write: %u", (unsigned)len);
    // prevent npe
    if ((p_out == nullptr && p_writer == nullptr) || (data == nullptr) ||
        (len == 0))
      return 0;

    // an ID3v2 header can only ever be located at the very start of the
    // stream, so it is only ever checked once, on the first bytes seen
    if (total_pos == 0) checkID3v2Header(data, len);

    // Bytes that turn out not to belong to the leading ID3v2 tag are held
    // back in a small trailing ring buffer: this way a byte is only ever
    // forwarded once we know it is not part of a trailing ID3v1/TAG+ tag
    // (which is only recognizable once we reach the real end of stream).
    // This avoids treating a coincidental "TAG"/"ID3" byte sequence inside
    // the actual audio data as metadata.
    out_buf.resize(len);
    out_buf.reset();
    for (size_t j = 0; j < len; j++) {
      total_pos++;
      if (id3v2_skip_remaining > 0) {
        id3v2_skip_remaining--;
        continue;
      }
      if (tail.isFull()) {
        uint8_t old = 0;
        tail.read(old);
        out_buf.write(old);
      }
      tail.write(data[j]);
    }

    size_t to_write = out_buf.available();
    if (to_write > 0) {
      LOGI("output: %u", (unsigned)to_write);
      writeOut(out_buf.data(), to_write);
    } else {
      LOGI("output ignored");
    }

    return len;
  }

 protected:
  Print *p_out = nullptr;
  AudioWriter *p_writer = nullptr;
  /// total number of input bytes seen so far (absolute stream position)
  uint64_t total_pos = 0;
  /// remaining bytes of a leading ID3v2 tag still to be dropped
  int id3v2_skip_remaining = 0;

  static const int kID3v1Len = 128;
  static const int kID3v1EnhancedLen = 227;
  static const int kMaxTailLen = kID3v1Len + kID3v1EnhancedLen;
  /// delay line holding the last (at most) kMaxTailLen bytes: needed because
  /// a trailing ID3v1/TAG+ tag can only be recognized once we know we are
  /// at the actual end of the stream
  RingBuffer<uint8_t> tail{kMaxTailLen};
  /// reused scratch buffer for the bytes flushed out of the tail per write()
  SingleBuffer<uint8_t> out_buf;

  /// ID3 verion 2 TAG Header (10 bytes)
  struct ID3v2Header {
    uint8_t header[3];  // ID3
    uint8_t version[2];
    uint8_t flags;
    uint8_t size[4];
  };

  size_t writeOut(const uint8_t *data, size_t len) {
    if (p_out) return p_out->write(data, len);
    if (p_writer) return p_writer->write(data, len);
    return 0;
  }

  /// checks for a ID3v2 header at the start of the stream and, if found,
  /// arranges for its bytes (header + body) to be skipped
  void checkID3v2Header(const uint8_t *data, size_t len) {
    if (len < sizeof(ID3v2Header)) return;
    if (data[0] == 'I' && data[1] == 'D' && data[2] == '3') {
      ID3v2Header hdr;
      memcpy(&hdr, data, sizeof(hdr));
      id3v2_skip_remaining =
          (int)sizeof(ID3v2Header) + (int)calcSizeID3v2(hdr.size);
      LOGI("ignoring ID3v2 header at stream start, len: %d",
           id3v2_skip_remaining);
    }
  }

  /// checks the buffered tail (the real end of the stream) for a trailing
  /// ID3v1 ("TAG") tag, optionally preceded by an enhanced ("TAG+") tag,
  /// and writes out whatever is left over as regular audio data
  void flushTail() {
    int n = tail.available();
    if (n <= 0) return;
    uint8_t buf[kMaxTailLen];
    tail.readArray(buf, n);

    int drop_from = n;
    if (n >= kID3v1Len && buf[n - kID3v1Len] == 'T' &&
        buf[n - kID3v1Len + 1] == 'A' && buf[n - kID3v1Len + 2] == 'G') {
      drop_from = n - kID3v1Len;
      if (drop_from >= kID3v1EnhancedLen) {
        int e = drop_from - kID3v1EnhancedLen;
        if (buf[e] == 'T' && buf[e + 1] == 'A' && buf[e + 2] == 'G' &&
            buf[e + 3] == '+') {
          drop_from = e;
        }
      }
    }

    if (drop_from > 0) {
      LOGI("output: %d", drop_from);
      writeOut(buf, drop_from);
    }
    if (drop_from < n) {
      LOGI("ignoring trailing tag, len: %d", n - drop_from);
    }
  }

  // calculate the synch save size for ID3v2
  uint32_t calcSizeID3v2(uint8_t chars[4]) {
    uint32_t byte0 = chars[0];
    uint32_t byte1 = chars[1];
    uint32_t byte2 = chars[2];
    uint32_t byte3 = chars[3];
    return byte0 << 21 | byte1 << 14 | byte2 << 7 | byte3;
  }
};

/**
 * @brief MetaDataFiler applied to the indicated decoder: Class which filters
 * out ID3v1 and ID3v2 Metadata and provides only the audio data to the decoder
 * @ingroup metadata
 * @ingroup codecs
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MetaDataFilterDecoder : public AudioDecoder {
 public:
  MetaDataFilterDecoder(AudioDecoder &decoder) {
    p_decoder = &decoder;
    filter.setOutput(decoder);
  }

  bool begin() override {
    is_active = true;
    filter.begin();
    p_decoder->begin();
    return AudioDecoder::begin();
  }

  void end() override {
    is_active = false;
    filter.end();
    AudioDecoder::end();
  }

  size_t write(const uint8_t *data, size_t len) override {
    return filter.write(data, len);
  }

  void setOutput(AudioStream &out_stream) override {
    p_decoder->setOutput(out_stream);
    addNotifyAudioChange(out_stream);
  }

  virtual void setOutput(AudioOutput &out_stream) override {
    p_decoder->setOutput(out_stream);
    addNotifyAudioChange(out_stream);
  }

  /// Defines where the decoded result is written to
  virtual void setOutput(Print &out_stream) override {
    p_decoder->setOutput(out_stream);
  }

  operator bool() override { return p_print != nullptr && is_active; }

 protected:
  AudioDecoder *p_decoder = nullptr;
  MetaDataFilter filter;
  bool is_active = false;
};

/**
 * @brief MetaDataFilter applied to an encoder: filters out ID3v1 / ID3v2
 * metadata from the mp3 input stream before it is provided to the wrapped
 * encoder. This is basically a pass-through for mp3 data with metadata skipped.
 * @ingroup metadata
 * @ingroup codecs
 */
class MetaDataFilterEncoder : public AudioEncoder {
 public:
  MetaDataFilterEncoder(AudioEncoder &encoder) {
    p_encoder = &encoder;
    filter.setOutput(encoder);
  }

  bool begin(AudioInfo info) override {
    setAudioInfo(info);
    return begin();
  }

  bool begin() override { return p_encoder->begin() && filter.begin(); }

  void end() override {
    p_encoder->end();
    filter.end();
  }

  void setAudioInfo(AudioInfo info) override { p_encoder->setAudioInfo(info); }

  void setOutput(Print &out_stream) override {
    p_encoder->setOutput(out_stream);
  }

  size_t write(const uint8_t *data, size_t len) override {
    return filter.write(data, len);
  }

  operator bool() override { return (bool)(*p_encoder); }

  const char *mime() override { return p_encoder->mime(); }

  uint32_t frameDurationUs() override { return p_encoder->frameDurationUs(); }

  uint16_t samplesPerFrame() override { return p_encoder->samplesPerFrame(); }

 protected:
  AudioEncoder *p_encoder = nullptr;
  MetaDataFilter filter;
  bool is_initialized = false;
};

}  // namespace audio_tools