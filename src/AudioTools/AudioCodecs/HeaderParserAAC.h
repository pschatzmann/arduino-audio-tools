#pragma once
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"

namespace audio_tools {

/**
 * @brief  AAC header parser to check if the data is a valid ADTS aac which
 * can extract some relevant audio information.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class HeaderParserAAC {
  class AACHeader {
   public:
    uint8_t id;
    uint8_t layer;
    bool protection_absent;
    uint8_t profile;
    uint8_t sampling_freq_idx;
    bool private_bit;
    uint8_t channel_cfg;
    bool original_copy;
    // uint8_t home; // not used
    uint8_t copyright_id_bit;
    uint8_t copyright_id_start;
    int frame_length;
    uint8_t adts_buf_fullness;  // Used for bit reservoir, 0x7FF = VBR
    int num_rawdata_blocks;     // Usually 00 (1 AAC frame per ADTS frame)
  };

 public:
  /// parses the header string and returns true if this is a valid mp3 file
  bool isValid(const uint8_t* data, int len) {
    if (findSyncWord(data, len) == -1) {
      LOGE("Could not find FrameSync");
      return false;
    }

    header.id = (data[1] >> 3) & 0b1;
    header.layer = (data[1] >> 1) & 0b11;
    header.protection_absent = (data[1]) & 0b1;
    header.profile = (data[2] >> 6) & 0b11;
    header.sampling_freq_idx = (data[2] >> 2) & 0b1111;
    header.private_bit = (data[2] >> 1) & 0b1;
    header.channel_cfg = ((data[2] & 0b1) << 2) | (data[3] >> 6);
    header.original_copy = (data[3] >> 5) & 0b1;
    // header.home = (data[3] >> 4) & 0b1; // unused
    //  parse adts_variable_header()
    header.copyright_id_bit = (data[3] >> 3) & 0b1;
    header.copyright_id_start = (data[3] >> 2) & 0b1;
    header.frame_length =
        ((data[3] & 0b11) << 11) | (data[4] << 3) | (data[5] >> 5);
    header.adts_buf_fullness = ((data[5] & 0b11111) << 6) | (data[6] >> 2);
    header.num_rawdata_blocks =
        (data[6]) & 0b11;  // Usually 00 (1 AAC frame per ADTS frame)

    bool is_valid = true;

    is_valid = is_valid && header.sampling_freq_idx < 12;
    is_valid = is_valid && header.channel_cfg < 8 && header.channel_cfg > 0;
    is_valid = is_valid && header.profile < 3;  // 0: Main, 1: LC, 2: SSR
    is_valid = is_valid && header.layer == 0;   // 0: ADTS
    is_valid = is_valid && header.id <= 1;      // 0: MPEG-4, 1: MPEG-2

    if (len >= header.frame_length + 11) {
      is_valid = is_valid && findSyncWord(data + header.frame_length,
                                          len - header.frame_length) != 0;
    }
    return is_valid;
  }

  uint16_t getSampleRate() const {
    return sample_rates[header.sampling_freq_idx];
  }

  uint8_t getChannels() const { return header.channel_cfg; }

  /// Determines the frame length
  int getFrameLength() { return header.frame_length; }

  /// Finds the mp3/aac sync word
  int findSyncWord(const uint8_t* buf, size_t nBytes, uint8_t synch = 0xFF,
                   uint8_t syncl = 0xF0) {
    for (int i = 0; i < nBytes - 1; i++) {
      if ((buf[i + 0] & synch) == synch && (buf[i + 1] & syncl) == syncl)
        return i;
    }
    return -1;
  }
  int getRawDataBlocks() { return header.num_rawdata_blocks; }

  AACHeader getHeader() { return header; }

 protected:
  const int sample_rates[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000,
                                22050, 16000, 12000, 11025, 8000,  7350};

  AACHeader header{0};
};

}  // namespace audio_tools