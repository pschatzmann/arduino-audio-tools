#include "AACDecoderHelix.h"
#include "AudioCodecs/AudioCodecsBase.h"

namespace audio_tools {

#ifndef SYNCWORDH
#define SYNCWORDH 0xff
#define SYNCWORDL 0xf0
#endif

struct ADTSParser {
  const int adtsSamplingRates[13] = {96000, 88200, 64000, 48000, 44100,
                                   32000, 24000, 22050, 16000, 12000,
                                   11025, 8000,  7350};

  bool is_valid = false;
  uint16_t syncword;
  uint8_t id;
  uint8_t layer;
  uint8_t protection_absent;
  uint8_t profile;
  uint8_t sampling_freq_idx;
  uint8_t private_bit;
  uint8_t channel_cfg;
  uint8_t original_copy;
  uint8_t home;
  uint8_t copyright_id_bit;
  uint8_t copyright_id_start;
  uint16_t frame_length;
  uint8_t adts_buf_fullness;
  uint8_t num_rawdata_blocks;
  uint32_t quick_check = 0;

  bool begin() { quick_check = 0; return true;}

  bool parse(uint8_t *hdr) {
    syncword = (hdr[0] << 4) | (hdr[1] >> 4);
    // parse fixed header
    id = (hdr[1] >> 3) & 0b1;
    layer = (hdr[1] >> 1) & 0b11;
    protection_absent = (hdr[1]) & 0b1;
    profile = (hdr[2] >> 6) & 0b11;
    sampling_freq_idx = (hdr[2] >> 2) & 0b1111;
    private_bit = (hdr[2] >> 1) & 0b1;
    channel_cfg = ((hdr[2] & 0x01) << 2) | ((hdr[3] & 0xC0) >> 6);
    original_copy = (hdr[3] >> 5) & 0b1;
    home = (hdr[3] >> 4) & 0b1;
    // parse variable header
    copyright_id_bit = (hdr[3] >> 3) & 0b1;
    copyright_id_start = (hdr[3] >> 2) & 0b1;
    // frame_length = ((hdr[3] & 0b11) << 11) | (hdr[4] << 3) | (hdr[5] >> 5);
    frame_length = ((((unsigned int)hdr[3] & 0x3)) << 11) |
                   (((unsigned int)hdr[4]) << 3) | (hdr[5] >> 5);
    adts_buf_fullness = ((hdr[5] & 0b11111) << 6) | (hdr[6] >> 2);
    num_rawdata_blocks = (hdr[6]) & 0b11;

    LOGD("id:%d layer:%d profile:%d freq:%d channel:%d frame_length:%d", id,
         layer, profile, rate(), channel_cfg,
         frame_length);

    // check
    is_valid = true;
    if (syncword != 0b111111111111) {
      is_valid = false;
    }
    if (id > 6) {
      LOGD("- Invalid id");
      is_valid = false;
    }
    if (sampling_freq_idx > 0xb) {
      LOGD("- Invalid sampl.freq");
      is_valid = false;
    }
    if (channel_cfg > 2) {
      LOGD("- Invalid channels");
      is_valid = false;
    }
    if (frame_length > 1024) {
      LOGD("- Invalid frame_length");
      is_valid = false;
    }
    if (!is_valid) {
      LOGD("=> Invalid ADTS");
    }
    return is_valid;
  }

  unsigned int size() { return frame_length; };

  void log() {
    LOGI("%s id:%d layer:%d profile:%d freq:%d channel:%d frame_length:%d",
         is_valid ? "+" : "-", id, layer, profile,
         rate(), channel_cfg, frame_length);
  }
  
  int rate(){
    return sampling_freq_idx>12? sampling_freq_idx : adtsSamplingRates[sampling_freq_idx];
  }

  bool isSyncWord(uint8_t *buf) {
    return ((buf[0] & SYNCWORDH) == SYNCWORDH &&
            (buf[1] & SYNCWORDL) == SYNCWORDL);
  }

  int findSynchWord(unsigned char *buf, int nBytes, int start = 0) {
    /* find byte-aligned syncword (12 bits = 0xFFF) */
    for (int i = start; i < nBytes - 1; i++) {
      if (isSyncWord(buf + i)) return i;
    }
    return -1;
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
  bool begin() override {
    parser.begin();
    buffer_write_size = 0;
    return true;
  }

  void end() override { buffer.resize(0); }

  /// Write AAC data to decoder
  size_t write(const uint8_t *data, size_t len) override {
    LOGD("AACDecoderADTS::write: %d", (int)len);

    // make sure that we can hold at least the len
    if (buffer.size() < len) {
      buffer.resize(len);
    }

    // write data to buffer
    size_t result = buffer.writeArray(data, len);
    LOGD("buffer size: %d", buffer.available());

    // process open bytes
    if (buffer_write_size == 0) {
      parseBuffer();
    } else {
      // process open frame
      if (buffer.available() >= buffer_write_size) {
        // write out data
        assert(buffer_write_size == parser.size());
        writeFrame();
        buffer_write_size = 0;
      }
    }
    return result;
  }

  /// checks if the class is active
  operator bool() override { return true; }

 protected:
  SingleBuffer<uint8_t> buffer{DEFAULT_BUFFER_SIZE};
  ADTSParser parser;
  int buffer_write_size = 0;

  void parseBuffer() {
    // when nothing is open
    while (buffer.available() >= 7 && buffer_write_size == 0) {
      int pos = parser.findSynchWord(buffer.data(), buffer.available());
      LOGD("synchword at %d from %d", pos, buffer.available());
      if (pos >= 0) {
        processSync(pos);
      } else {
        // if no sync word was found
        int to_delete = max(7, buffer.available());
        buffer.clearArray(to_delete);
        LOGW("Removed invalid %d bytes", to_delete);
      }
    }
  }

  void processSync(int pos) {
    // remove data up to the sync word
    buffer.clearArray(pos);
    LOGD("Removing %d", pos);
    assert(parser.isSyncWord(buffer.data()));
    // the header needs 7 bytes
    if (buffer.available() < 7) {
      return;
    }

    if (parser.parse(buffer.data())) {
      processValidFrame();
    } else {
      // header not valid -> remove current synch word
      buffer.clearArray(2);
      LOGD("Removing invalid synch to restart scanning: %d", buffer.available());
    }
  }

  void processValidFrame() {
    resizeBuffer();
    if (buffer.available() >= parser.size()) {
      writeFrame();
    } else {
      LOGD("Expecting more data up to %d", parser.size());
      // we must load more data
      buffer_write_size = parser.size();
    }
  }

  void writeFrame() {
    // write out data
    parser.log();
    int size = parser.size();
    if (size>0){
      LOGD("writing ADTS Frame: %d bytes", size);
      assert(buffer.available() >= size);
      size_t len = p_print->write(buffer.data(), size);
      assert(len == size);
      buffer.clearArray(parser.size());
    } else {
      buffer.clearArray(2);
    }
  }

  void resizeBuffer() {
    if (parser.size() > buffer.size()) {
      LOGI("resize buffer %d to %d", (int)buffer.size(), (int)parser.size());
      buffer.resize(parser.size());
      buffer_write_size = parser.size();
    }
  }
};

}  // namespace audio_tools
