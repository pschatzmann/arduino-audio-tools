#pragma once
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"

namespace audio_tools {

#ifndef SYNCWORDH
#define SYNCWORDH 0xff
#define SYNCWORDL 0xf0
#endif

#define ERROR_FMT_CHANGE "- Invalid ADTS change: %s"
#define ERROR_FMT "- Invalid ADTS: %s (0x%x)"

/**
 * @brief Structure to hold ADTS header field values
 */

class ADTSParser {
 public:
  struct ADTSHeader {
    uint16_t syncword = 0;
    uint8_t id = 0;
    uint8_t layer = 0;
    uint8_t protection_absent = 0;
    uint8_t profile = 0;
    uint8_t sampling_freq_idx = 0;
    uint8_t private_bit = 0;
    uint8_t channel_cfg = 0;
    uint8_t original_copy = 0;
    uint8_t home = 0;
    uint8_t copyright_id_bit = 0;
    uint8_t copyright_id_start = 0;
    uint16_t frame_length = 0;
    uint8_t adts_buf_fullness = 0;
    uint8_t num_rawdata_blocks = 0;
  };

  bool begin() {
    is_first = true;
    is_valid = true;
    return true;
  }

  bool parse(uint8_t *hdr) {
    header.syncword = (hdr[0] << 4) | (hdr[1] >> 4);
    // parse fixed header
    header.id = (hdr[1] >> 3) & 0b1;
    header.layer = (hdr[1] >> 1) & 0b11;
    header.protection_absent = (hdr[1]) & 0b1;
    header.profile = (hdr[2] >> 6) & 0b11;
    header.sampling_freq_idx = (hdr[2] >> 2) & 0b1111;
    header.private_bit = (hdr[2] >> 1) & 0b1;
    header.channel_cfg = ((hdr[2] & 0x01) << 2) | ((hdr[3] & 0xC0) >> 6);
    header.original_copy = (hdr[3] >> 5) & 0b1;
    header.home = (hdr[3] >> 4) & 0b1;
    // parse variable header
    header.copyright_id_bit = (hdr[3] >> 3) & 0b1;
    header.copyright_id_start = (hdr[3] >> 2) & 0b1;
    header.frame_length = ((((unsigned int)hdr[3] & 0x3)) << 11) |
                          (((unsigned int)hdr[4]) << 3) | (hdr[5] >> 5);
    header.adts_buf_fullness = ((hdr[5] & 0b11111) << 6) | (hdr[6] >> 2);
    header.num_rawdata_blocks = (hdr[6]) & 0b11;

    LOGD("id:%d layer:%d profile:%d freq:%d channel:%d frame_length:%d",
         header.id, header.layer, header.profile, getSampleRate(),
         header.channel_cfg, header.frame_length);

    // check
    is_valid = check();
    return is_valid;
  }

  uint32_t getFrameLength() { return header.frame_length; };

  void log() {
    LOGI("%s id:%d layer:%d profile:%d freq:%d channel:%d frame_length:%d",
         is_valid ? "+" : "-", header.id, header.layer, header.profile,
         getSampleRate(), header.channel_cfg, header.frame_length);
  }

  int getSampleRate() {
    return header.sampling_freq_idx > 12
               ? header.sampling_freq_idx
               : (int)adtsSamplingRates[header.sampling_freq_idx];
  }

  bool isSyncWord(const uint8_t *buf) {
    return ((buf[0] & SYNCWORDH) == SYNCWORDH &&
            (buf[1] & SYNCWORDL) == SYNCWORDL);
  }

  int findSyncWord(const uint8_t *buf, int nBytes, int start = 0) {
    /* find byte-aligned syncword (12 bits = 0xFFF) */
    for (int i = start; i < nBytes - 1; i++) {
      if (isSyncWord(buf + i)) return i;
    }
    return -1;
  }

  ADTSHeader &data() { return header; }

 protected:
  const int adtsSamplingRates[13] = {96000, 88200, 64000, 48000, 44100,
                                     32000, 24000, 22050, 16000, 12000,
                                     11025, 8000,  7350};

  ADTSHeader header;
  ADTSHeader header_ref;
  bool is_first = true;
  bool is_valid = false;

  bool check() {
    if (header.syncword != 0b111111111111) {
      LOGW(ERROR_FMT, "sync", (int)header.syncword);
      is_valid = false;
    }
    if (header.id > 6) {
      LOGW(ERROR_FMT, "id", (int)header.id);
      is_valid = false;
    }
    if (header.sampling_freq_idx > 0xb) {
      LOGW(ERROR_FMT, "freq", (int)header.sampling_freq_idx);
      is_valid = false;
    }
    // valid value 0-7
    // if (header.channel_cfg == 0 || header.channel_cfg > 7) {
    if (header.channel_cfg > 7) {
      LOGW(ERROR_FMT, "channels", (int)header.channel_cfg);
      is_valid = false;
    }
    if (header.frame_length > 8191) {  // tymically <= 768
      LOGW(ERROR_FMT, "frame_length", (int)header.frame_length);
      is_valid = false;
    }
    // on subsequent checks we need to compare with the first header
    if (!is_first) {
      is_valid = checkRef();
    }
    if (is_valid) {
      is_first = false;
      header_ref = header;
    }
    return is_valid;
  }

  bool checkRef() {
    char msg[200] = "";
    bool is_valid = true;
    if (header.id != header_ref.id) {
      strcat(msg, "id ");
      is_valid = false;
    }
    if (header.layer != header_ref.layer) {
      strcat(msg, "layer ");
      is_valid = false;
    }
    if (header.profile != header_ref.profile) {
      strcat(msg, "profile ");
      is_valid = false;
    }
    if (header.sampling_freq_idx != header_ref.sampling_freq_idx) {
      strcat(msg, "freq ");
      is_valid = false;
    }
    if (header.channel_cfg != header_ref.channel_cfg) {
      strcat(msg, "channel ");
      is_valid = false;
    }
    if (header.adts_buf_fullness != header_ref.adts_buf_fullness) {
      strcat(msg, "fullness");
      is_valid = false;
    }
    if (!is_valid) {
      LOGW(ERROR_FMT_CHANGE, msg);
    }
    return is_valid;
  }
};

/**
 * @brief Audio Data Transport Stream (ADTS) is a format similar to Audio Data
 * Interchange Format (ADIF), used by MPEG TS or Shoutcast to stream audio
 * defined in MPEG-2 Part 7, usually AAC. This parser extracts all valid ADTS
 * frames from the data stream ignoring other data.
 *
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ADTSDecoder : public AudioDecoder {
 public:
  ADTSDecoder() = default;
  ADTSDecoder(AudioDecoder &dec) { p_dec = &dec; };

  bool begin() override {
    parser.begin();
    if (p_dec) p_dec->begin();
    return true;
  }

  void end() override {
    parseBuffer();
    writeData(out_buffer.data(), out_buffer.available());
    out_buffer.reset();
    buffer.resize(0);
    if (p_dec) p_dec->end();
  }

  /// Write AAC data to decoder
  size_t write(const uint8_t *data, size_t len) override {
    LOGI("AACDecoderADTS::write: %d", (int)len);

    parseBuffer();

    // write data to buffer
    size_t result = buffer.writeArray(data, len);
    // assert(result == len);
    LOGD("buffer size: %d", buffer.available());

    return result;
  }

  /// checks if the class is active
  operator bool() override { return true; }

  /// By default we write the parsed frames directly to the output:
  /// alternatively you can activate a buffer here
  void setOutputBufferSize(int size) { out_buffer.resize(size); }

  /// Defines the parse buffer size: default is 1024
  void setParseBufferSize(int size) { buffer.resize(size); }

  /// Defines where the decoded result is written to
  void setOutput(AudioStream &out_stream) override {
    if (p_dec) {
      p_dec->setOutput(out_stream);
    } else {
      AudioDecoder::setOutput(out_stream);
    }
  }

  /// Defines where the decoded result is written to
  void setOutput(AudioOutput &out_stream) override {
    if (p_dec) {
      p_dec->setOutput(out_stream);
    } else {
      AudioDecoder::setOutput(out_stream);
    }
  }

  /// Defines where the decoded result is written to
  void setOutput(Print &out_stream) override {
    if (p_dec) {
      p_dec->setOutput(out_stream);
    } else {
      AudioDecoder::setOutput(out_stream);
    }
  }

 protected:
  SingleBuffer<uint8_t> buffer{DEFAULT_BUFFER_SIZE};
  SingleBuffer<uint8_t> out_buffer;
  ADTSParser parser;
  AudioDecoder *p_dec = nullptr;

  void parseBuffer() {
    TRACED();

    // Need at least 7 bytes for a valid ADTS header
    while (true) {
      if (buffer.available() <= 5) return;
      // Needs to contain sync word
      int syncPos = parser.findSyncWord(buffer.data(), buffer.available());
      if (syncPos < 0) {
        return;
      }
      // buffer needs to start with sync word
      if (syncPos > 0) {
        buffer.clearArray(syncPos);
        LOGI("Cleared %d bytes", syncPos);
      }
      // assert(parser.findSyncWord(buffer.data(), buffer.available()) == 0);
      //  Try to parse the header
      if (parser.parse(buffer.data())) {
        // Get the frame length which includes the header
        uint16_t frameLength = parser.getFrameLength();
        if (frameLength > buffer.available()) {
          // not enough data
          return;
        }
        // write data to decoder
        if (out_buffer.size() > 0) {
          writeDataBuffered(buffer.data(), frameLength);
        } else {
          writeData(buffer.data(), frameLength);
        }
        buffer.clearArray(frameLength);
      } else {
        LOGI("Invalid ADTS header");
        // ignore data and move to next synch word
        int pos = parser.findSyncWord(buffer.data(), buffer.available(), 5);
        if (pos < 0) {
          // no more sync word found
          buffer.reset();
        } else {
          buffer.clearArray(pos);
        }
      }
    }
  }

  size_t writeDataBuffered(uint8_t *data, size_t size) {
    LOGI("writeDataBuffered: %d", (int)size);
    for (int j = 0; j < size; j++) {
      out_buffer.write(data[j]);
      if (out_buffer.isFull()) {
        writeData(out_buffer.data(), out_buffer.available());
        out_buffer.reset();
      }
    }
    return size;
  }
  size_t writeData(uint8_t *data, size_t size) {
    LOGI("writeData: %d", (int)size);
    if (p_print) {
      size_t len = ::writeData<uint8_t>(p_print, data, size);
      assert(len == size);
      return (len == size);
    }
    if (p_dec) {
      LOGI("write to decoder: %d", (int)size);
      size_t len = writeDataT<uint8_t, AudioDecoder>(p_dec, data, size);
      assert(len == size);
      return (len == size);
    }
    return 0;
  }
};

}  // namespace audio_tools
