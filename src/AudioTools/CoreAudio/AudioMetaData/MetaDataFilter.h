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
    start = 0;
    if (p_writer) p_writer->begin();
    return true;
  }

  void end() override {
    if (p_writer) p_writer->end();
  }

  /// Writes the data to the decoder
  size_t write(const uint8_t *data, size_t len) override {
    LOGI("write: %u", len);
    size_t result = len;
    // prevent npe
    if ((p_out == nullptr && p_writer == nullptr) || (data == nullptr) ||
        (len == 0))
      return 0;

    // find tag
    int meta_len = 0;
    if (findTag(data, len, metadata_range.from, meta_len)) {
      current_pos = 0;
      metadata_range.setLen(meta_len);
      LOGI("ignoring metadata at pos: %d len: %d", metadata_range.from,
           meta_len);
    }

    // nothing to ignore
    if (!metadata_range.isDefined()) {
      if (p_out) return p_out->write(data, len);
      if (p_writer) return p_writer->write(data, len);
    }

    // ignore data in metadata range
    SingleBuffer<uint8_t> tmp(len);
    for (int j = 0; j < len; j++) {
      if (!metadata_range.inRange(current_pos)) {
        tmp.write(data[j]);
      }
      current_pos++;
    }

    // write partial data
    size_t to_write = tmp.available();
    if (to_write > 0) {
      LOGI("output: %d", to_write);
      size_t written = 0;
      if (p_out) written = p_out->write(tmp.data(), to_write);
      if (p_writer) written = p_writer->write(tmp.data(), to_write);
      assert(to_write == written);
      metadata_range.clear();
    } else {
      LOGI("output ignored");
    }

    // reset for next run
    if (current_pos > metadata_range.to) {
      current_pos = 0;
      metadata_range.clear();
    }

    return result;
  }

 protected:
  Print *p_out = nullptr;
  AudioWriter *p_writer = nullptr;
  int current_pos = 0;
  enum MetaType { TAG, TAG_PLUS, ID3 };
  int start = 0;
  /// Metadata range
  struct Range {
    int from = -1;
    int to = -1;

    bool inRange(int pos) { return pos >= from && pos < to; }
    void setLen(int len) { to = from + len; }

    void clear() {
      from = -1;
      to = -1;
    }
    bool isDefined() { return from != -1; }
  } metadata_range;

  /// ID3 verion 2 TAG Header (10 bytes)
  struct ID3v2 {
    uint8_t header[3];  // ID3
    uint8_t version[2];
    uint8_t flags;
    uint8_t size[4];
  } tagv2;

  /// determines if the data conatins a ID3v1 or ID3v2 tag
  bool findTag(const uint8_t *data, size_t len, int &pos_tag, int &meta_len) {
    MetaType tag_type;
    if (find((const char *)data, len, pos_tag, tag_type)) {
      switch (tag_type) {
        case TAG:
          LOGD("TAG");
          meta_len = 128;
          break;
        case TAG_PLUS:
          LOGD("TAG+");
          meta_len = 227;
          break;
        case ID3:
          LOGD("ID3");
          memcpy(&tagv2, data + pos_tag, sizeof(ID3v2));
          meta_len = calcSizeID3v2(tagv2.size);
          break;
      }
      return true;
    }
    return false;
  }

  // calculate the synch save size for ID3v2
  uint32_t calcSizeID3v2(uint8_t chars[4]) {
    uint32_t byte0 = chars[0];
    uint32_t byte1 = chars[1];
    uint32_t byte2 = chars[2];
    uint32_t byte3 = chars[3];
    return byte0 << 21 | byte1 << 14 | byte2 << 7 | byte3;
  }

  /// find the tag position in the string;
  bool find(const char *str, size_t len, int &pos, MetaType &type) {
    if (str == nullptr || len <= 0) return false;
    for (size_t j = 0; j <= len - 3; j++) {
      if (str[j] == 'T' && str[j + 1] == 'A' && str[j + 2] == 'G') {
        type = str[j + 3] == '+' ? TAG_PLUS : TAG;
        pos = j;
        return true;
      } else if (str[j] == 'I' && str[j + 1] == 'D' && str[j + 2] == '3') {
        type = ID3;
        pos = j;
        return true;
      }
    }
    return false;
  }
};

/***
 * MetaDataFiler applied to the indicated decoder: Class which filters out ID3v1
 * and ID3v2 Metadata and provides only the audio data to the decoder
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
  }

  virtual void setOutput(AudioOutput &out_stream) override {
    p_decoder->setOutput(out_stream);
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

}  // namespace audio_tools