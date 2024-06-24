
#pragma once

#include "AudioBasic/Str.h"
#include "AudioCodecs/AudioCodecsBase.h"

namespace audio_tools {


enum Base46Logic { NoCR, CRforFrame, CRforWrite };
static char encoding_table[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};
static int mod_table[] = {0, 2, 1};

static const int B64index[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  62, 63, 62, 62, 63, 52, 53, 54, 55, 56, 57,
    58, 59, 60, 61, 0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,
    7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 0,  0,  0,  0,  63, 0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
    37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};

/**
 * @brief DecoderBase64 - Converts a Base64 encoded Stream into the original
 * data stream. Decoding only gives a valid result if we start at a limit of 4
 * bytes. We therefore use by default a newline to determine a valid start
 * boundary.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DecoderBase64 : public AudioDecoder {
 public:
  /**
   * @brief Constructor for a new DecoderBase64 object
   */

  DecoderBase64() { TRACED(); }

  /**
   * @brief Constructor for a new DecoderBase64 object
   *
   * @param out_buffeream Output Stream to which we write the decoded result
   */
  DecoderBase64(Print &out) {
    TRACED();
    setOutput(out);
  }

  /// Defines the output Stream
  void setOutput(Print &out) override { p_print = &out; }

  /// We expect new lines to delimit the individual lines
  void setNewLine(Base46Logic logic) { newline_logic = logic; }

  bool begin() override {
    TRACED();
    is_valid = newline_logic == NoCR;
    active = true;
    return true;
  }

  void end() override {
    TRACED();
    // deconde ramaining bytes
    int len = buffer.available();
    uint8_t tmp[len];
    buffer.readArray(tmp, len);
    decodeLine(tmp, len);

    active = false;
    buffer.resize(0);
  }

  size_t write(const uint8_t *data, size_t len) override {
    if (p_print == nullptr) return 0;
    TRACED();
    addToBuffer((uint8_t *)data, len);
    int decode_size = 4;  // maybe we should increase this ?
    while (buffer.available() >= decode_size) {
      uint8_t tmp[decode_size];
      buffer.readArray(tmp, decode_size);
      decodeLine(tmp, decode_size);
    }

    return len;
  }

  operator bool() override { return active; }

 protected:
  bool active = false;
  bool is_valid = false;
  Base46Logic newline_logic = CRforFrame;
  Vector<uint8_t> result;
  RingBuffer<uint8_t> buffer{1500};
  AudioInfo info;

  void decodeLine(uint8_t *data, size_t byteCount) {
    LOGD("decode: %d", (int)byteCount);
    int len = byteCount;

    unsigned char *p = (unsigned char *)data;
    int pad = len > 0 && (len % 4 || p[len - 1] == '=');
    const size_t L = ((len + 3) / 4 - pad) * 4;
    result.resize(L / 4 * 3 + pad);
    memset(result.data(), 0, result.size());

    for (size_t i = 0, j = 0; i < L; i += 4) {
      int32_t n = static_cast<int32_t>(B64index[p[i]]) << 18 | B64index[p[i + 1]] << 12 |
              B64index[p[i + 2]] << 6 | B64index[p[i + 3]];
      result[j++] = n >> 16;
      result[j++] = n >> 8 & 0xFF;
      result[j++] = n & 0xFF;
    }
    if (pad) {
      int32_t n = static_cast<int32_t>(B64index[p[L]]) << 18 | B64index[p[L + 1]] << 12;
      result[result.size() - 1] = n >> 16;

      if (len > L + 2 && p[L + 2] != '=') {
        n |= B64index[p[L + 2]] << 6;
        result.push_back(n >> 8 & 0xFF);
      }
    }
    writeBlocking(p_print, result.data(), result.size());
  }

  void addToBuffer(uint8_t *data, size_t len) {
    TRACED();
    if (buffer.size() < len) {
      buffer.resize(len);
    }
    // syncronize to find a valid start position
    int start = 0;
    if (!is_valid) {
      for (int j = 0; j < len; j++) {
        if (data[j] == '\n') {
          start = j;
          is_valid = true;
          break;
        }
      }
    }

    if (is_valid) {
      // remove white space
      for (int j = start; j < len; j++) {
        if (!isspace(data[j])) {
          buffer.write(data[j]);
        } else if (data[j] == '\n') {
          int offset = buffer.available() % 4;
          if (offset > 0) {
            LOGW("Resync %d (-%d)...", buffer.available(), offset);
            uint8_t tmp[4];
            buffer.readArray(tmp, offset);
          }
        }
      }
    }
    LOGD("buffer: %d, is_valid: %s", buffer.available(),
         is_valid ? "true" : "false");
  }
};


/**
 * @brief EncoderBase64s - Encodes the input data into a Base64 string.
 * By default each audio frame is followed by a new line, so that we can
 * easily resynchronize the reading of a data stream. The generation
 * of the new line can be configured with the setNewLine() method.
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class EncoderBase64 : public AudioEncoder {
 public:
  // Empty Conbuffeructor - the output buffeream must be provided with begin()
  EncoderBase64() {}

  // Conbuffeructor providing the output buffeream
  EncoderBase64(Print &out) { p_print = &out; }

  /// Defines the output Stream
  void setOutput(Print &out_buffeream) override {
    p_print = &out_buffeream;
  }

  /// Provides "text/base64"
  const char *mime() override { return "text/base64"; }

  /// We add a new line after each write
  void setNewLine(Base46Logic flag) { newline_logic = flag; }

  /// starts the processing using the actual RAWAudioInfo
  virtual bool begin() override {
    is_open = true;
    frame_size = info.bits_per_sample * info.channels / 8;
    if (newline_logic != NoCR) {
      if (frame_size==0){
        LOGW("AudioInfo not defined");
        // assume frame size
        frame_size = 4;
      }
      p_print->write('\n');
      flush();
    }
    return true;
  }

  /// stops the processing
  void end() override { is_open = false; }

  /// Writes PCM data to be encoded as RAW
  virtual size_t write(const uint8_t *data, size_t len) override {
    LOGD("EncoderBase64::write: %d", (int)len);

    switch (newline_logic) {
      case NoCR:
      case CRforWrite:
        encodeLine(data, len);
        break;
      case CRforFrame: {
        int frames = len / frame_size;
        int open = len;
        int offset = 0;
        while (open > 0) {
          int write_size = min(frame_size, open);
          encodeLine(data + offset, write_size);
          open -= write_size;
          offset += write_size;
        }
        break;
      }
    }

    return len;
  }

  operator bool() override { return is_open; }

  bool isOpen() { return is_open; }

 protected:
  Print *p_print = nullptr;
  bool is_open;
  Base46Logic newline_logic = CRforFrame;
  Vector<uint8_t> ret;
  AudioInfo info;
  int frame_size;

  void flush() {
#if defined(ESP32) 
#  if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(3, 3, 5) 
      p_print->flush();
#  endif
#else
      p_print->flush();
#endif
  }

  void encodeLine(const uint8_t *data, size_t input_length) {
    LOGD("EncoderBase64::encodeLine: %d", (int)input_length);
    int output_length = 4 * ((input_length + 2) / 3);
    if (ret.size() < output_length + 1) {
      ret.resize(output_length + 1);
    }

    for (int i = 0, j = 0; i < input_length;) {
      uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
      uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
      uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

      uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

      ret[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
      ret[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
      ret[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
      ret[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (int i = 0; i < mod_table[input_length % 3]; i++)
      ret[output_length - 1 - i] = '=';

    // add a new line to the end
    if (newline_logic != NoCR) {
      ret[output_length] = '\n';
      output_length++;
    }

    writeBlocking(p_print, ret.data(), output_length);
    flush();
  }
};

}  // namespace audio_tools
