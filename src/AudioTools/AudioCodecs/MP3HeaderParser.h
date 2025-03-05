#pragma once
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"

namespace audio_tools {

/**
 * @brief  MP3 header parser to check if the data is a valid mp3 and
 * to extract some relevant audio information. We try to find some valid
 * frames with a valid sync in the beginning and the end.
 * See https://www.codeproject.com/KB/audio-video/mpegaudioinfo.aspx
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class MP3HeaderParser {
  // MPEG audio frame header
  // variables are declared in their serialized order
  // includes crc value
  struct __attribute__((packed)) FrameHeader {
    static const unsigned int SERIALIZED_SIZE = 4;

    // bitmasks for frame header fields grouped by byte
    static const unsigned char FRAMESYNC_FIRST_BYTEMASK = 0b11111111;

    static const unsigned char FRAMESYNC_SECOND_BYTEMASK = 0b1110000;
    static const unsigned char AUDIO_VERSION_MASK = 0b00011000;
    static const unsigned char LAYER_DESCRIPTION_MASK = 0b00000110;
    static const unsigned char PROTECTION_BIT_MASK = 0b00000001;

    static const unsigned char BITRATE_INDEX_MASK = 0b11110000;
    static const unsigned char SAMPLERATE_INDEX_MASK = 0b00001100;
    static const unsigned char PADDING_BIT_MASK = 0b00000010;
    static const unsigned char PRIVATE_BIT_MASK = 0b00000001;

    static const unsigned char CHANNEL_MODE_MASK = 0b11000000;
    static const unsigned char MODE_EXTENTION_MASK = 0b00110000;
    static const unsigned char COPYRIGHT_BIT_MASK = 0b00001000;
    static const unsigned char ORIGINAL_BIT_MASK = 0b00000100;
    static const unsigned char EMPHASIS_MASK = 0b00000011;

    char FrameSyncByte;
    bool FrameSyncBits : 3;

    // indicates MPEG standard version
    enum class AudioVersionID : unsigned {
      MPEG_2_5 = 0b00,
      INVALID = 0b01,  // reserved
      MPEG_2 = 0b10,
      MPEG_1 = 0b11,
    } AudioVersion : 2;

    // indicates which audio layer of the MPEG standard
    enum class LayerID : unsigned {
      INVALID = 0b00,  // reserved
      LAYER_3 = 0b01,
      LAYER_2 = 0b10,
      LAYER_1 = 0b11,
    } Layer : 2;

    // indicates whether theres a 16 bit crc checksum following the header
    bool Protection : 1;

    // sample & bitrate indexes meaning differ depending on MPEG version
    // use getBitRate() and GetSamplerate()
    bool BitrateIndex : 4;
    bool SampleRateIndex : 2;

    // indicates whether the audio data is padded with 1 extra byte (slot)
    bool Padding : 1;

    // this is only informative
    bool Private : 1;

    // indicates channel mode
    enum class ChannelModeID : unsigned {
      STEREO = 0b00,
      JOINT = 0b01,   // joint stereo
      DUAL = 0b10,    // dual channel (2 mono channels)
      SINGLE = 0b11,  // single channel (mono)
    } ChannelMode : 2;

    // Only used in joint channel mode. Meaning differ depending on audio layer
    // Use GetExtentionMode()
    bool ExtentionMode : 2;

    // indicates whether the audio is copyrighted
    bool Copyright : 1;

    // indicates whether the frame is located on the original media or a copy
    bool Original : 1;

    // indicates to the decoder that the file must be de-emphasized, ie the
    // decoder must 're-equalize' the sound after a Dolby-like noise supression.
    // It is rarely used.
    enum class EmphasisID : unsigned {
      NONE = 0b00,
      MS_50_15 = 0b01,
      INVALID = 0b10,
      CCIT_J17 = 0b10,
    } Emphasis : 2;

    enum SpecialBitrate {
      INVALID = -8000,
      ANY = 0,
    };

    signed int getBitRate() const {
      // version, layer, bit index
      static signed char rateTable[4][4][16] = {
          // version[00] = MPEG_2_5
          {
              // layer[00] = INVALID
              {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
              // layer[01] = LAYER_3
              {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 18, 20, -1},
              // layer[10] = LAYER_2
              {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 18, 20, -1},
              // layer[11] = LAYER_1
              {0, 4, 6, 7, 8, 10, 12, 14, 16, 18, 20, 22, 24, 28, 32, -1},
          },

          // version[01] = INVALID
          {
              {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
              {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
              {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
              {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
          },

          // version[10] = MPEG_2
          {
              // layer[00] = INVALID
              {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
              // layer[01] = LAYER_3
              {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 18, 20, -1},
              // layer[10] = LAYER_2
              {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 18, 20, -1},
              // layer[11] = LAYER_1
              {0, 4, 6, 7, 8, 10, 12, 14, 16, 18, 20, 22, 24, 28, 32, -1},
          },

          // version[11] = MPEG_1
          {
              // layer[00] = INVALID
              {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
              // layer[01] = LAYER_3
              {0, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, -1},
              // layer[10] = LAYER_2
              {0, 4, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, -1},
              // layer[11] = LAYER_1
              {0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, -1},
          },
      };
      signed char rate_byte = rateTable[AudioVersion][Layer][BitrateIndex];
      if (rate_byte == -1) {
        LOGE("Unsupported bitrate");
        return 0;
      }
      return rate_byte * 8000;
    }

    enum SpecialSampleRate {
      RESERVED = 0,
    };

    unsigned short getSampleRate() const {
      // version, sample rate index
      static unsigned short rateTable[4][4] = {
          // version[00] = MPEG_2_5
          {11025, 12000, 8000, 0},
          // version[01] = INVALID
          {0, 0, 0, 0},
          // version[10] = MPEG_2
          {22050, 24000, 16000, 0},
          // version[11] = MPEG_1
          {44100, 48000, 32000, 0},
      };

      return rateTable[AudioVersion][SampleRateIndex];
    }

    int getFrameLength() {
      int sample_rate = getSampleRate();
      if (sample_rate == 0) return 0;
      return int((144 * getBitRate() / sample_rate) + Padding);
    }
  };

 public:
  /// parses the header string and returns true if this is a valid mp3 file
  bool isValid(const uint8_t* data, int len) {
    memset(&header, 0, sizeof(header));

    // if we start with ID3 -> valid mp3
    if (memcmp(data, "ID3", 3) == 0) {
      LOGI("ID3 found");
      return true;
    }

    int sync_pos = seekFrameSync(data, len);
    if (sync_pos == -1) {
      LOGE("Could not find FrameSync");
      return false;
    }

    // xing header  -> valid mp3
    if (sync_pos >= 0 && contains(data, "Xing", len)) {
      LOGI("Xing found");
      return true;
    }

    // xing header  -> valid mp3
    if (sync_pos >= 0 && contains(data, "Info", len)) {
      LOGI("Xing Info found");
      return true;
    }

    // find valid segement in available data
    bool is_valid_mp3 = false;
    while (true) {
      LOGI("checking header at %d", sync_pos);
      int len_available = len - sync_pos;

      // check if we have enough data for header
      if (len_available < sizeof(header)) {
        LOGE("Not enough data to determine mp3 header");
        break;
      }

      readFrameHeader(data+sync_pos);
      is_valid_mp3 = validate(data + sync_pos, len_available);

      // check expected expected end of frame ( next frame)
      int frame_len = getFrameLength();
      if (is_valid_mp3 && frame_len > 0){
        int expected_next_frame =  sync_pos + getFrameLength();
        int pos = seekFrameSync(data + expected_next_frame, len - expected_next_frame);
        LOGI("- end frame found: %s", pos==0?"yes": "no");
        if (pos !=0)  is_valid_mp3 = false;
      }

      // find end sync
      int pos = seekFrameSync(data + sync_pos + 2, len_available - 2);
      // no more data to be validated
      if (pos == -1) break;
      // calculate new sync_pos
      sync_pos = pos + sync_pos + 2;

      // success and we found an end sync with a bit rate
      if (is_valid_mp3 && getSampleRate() != 0) break;
    }
    if (is_valid_mp3) {
      LOGI("-------------------");
      LOGI("is mp3: %s", is_valid_mp3 ? "yes" : "no");
      LOGI("frame size: %d", getFrameLength());
      LOGI("sample rate: %u", getSampleRate());
     // LOGI("bit rate index: %d", getFrameHeader().BitrateIndex);
      LOGI("bit rate: %d", getBitRate());
      LOGI("Padding: %d", getFrameHeader().Padding);
      LOGI("Layer: %s (0x%x)", getLayerStr(),(int) getFrameHeader().Layer);
      LOGI("Version: %s (0x%x)", getVersionStr(),
           (int)getFrameHeader().AudioVersion);
      LOGI("-------------------");
    }
    return is_valid_mp3;
  }

  uint16_t getSampleRate() const { return header.getSampleRate(); }

  int getBitRate() const { return header.getBitRate(); }

  /// Determines the frame length
  int getFrameLength() { return header.getFrameLength(); }

  /// Provides the estimated playing time in seconds based on the bitrate of the
  /// first segment
  size_t getPlayingTime(size_t fileSizeBytes) {
    int bitrate = getBitRate();
    if (bitrate == 0) return 0;
    return fileSizeBytes / bitrate;
  }

  const char* getVersionStr() const {
    return header.AudioVersion == FrameHeader::AudioVersionID::MPEG_1   ? "1"
           : header.AudioVersion == FrameHeader::AudioVersionID::MPEG_2 ? "2"
           : header.AudioVersion == FrameHeader::AudioVersionID::MPEG_2_5
               ? "2.5"
               : "INVALID";
  }

  const char* getLayerStr() const {
    return header.Layer == FrameHeader::LayerID::LAYER_1   ? "1"
           : header.Layer == FrameHeader::LayerID::LAYER_2 ? "2"
           : header.Layer == FrameHeader::LayerID::LAYER_3 ? "3"
                                                           : "INVALID";
  }

  // provides the parsed MP3 frame header
  FrameHeader getFrameHeader() { return header; }

  /// Finds the mp3/aac sync word
  int findSyncWord(const uint8_t* buf, size_t nBytes, uint8_t synch = 0xFF,
                   uint8_t syncl = 0xF0) {
    for (int i = 0; i < nBytes - 1; i++) {
      if ((buf[i + 0] & synch) == synch &&
          (buf[i + 1] & syncl) == syncl)
        return i;
    }
    return -1;
  }

 protected:
  FrameHeader header;

  bool validate(const uint8_t* data, size_t len) {
    assert(header.FrameSyncByte = 0xFF);
    // check end of frame: it must contains a sync word
    return FrameReason::VALID == validateFrameHeader(header);
  }

  bool contains(const uint8_t* data, const char* toFind, size_t len) {
    int find_str_len = strlen(toFind);
    for (int j = 0; j < len - find_str_len; j++) {
      if (memcmp(data + j, toFind, find_str_len) == 0) return true;
    }
    return false;
  }

  // Seeks to the byte at the end of the next continuous run of 11 set bits.
  //(ie. after seeking the cursor will be on the byte of which its 3 most
  // significant bits are part of the frame sync)
  int seekFrameSync(const uint8_t* str, size_t len) {
    char cur;
    for (int j = 0; j < len - 1; j++) {
      cur = str[j];
      // read bytes until EOF or a byte with all bits set is encountered
      if ((cur & 0b11111111) != 0b11111111) continue;

      if ((str[j + 1] & 0b11100000) != 0b11100000) {
        // if the next byte does not have its 3 most significant bits set it is
        // not the end of the framesync and it also cannot be the start of a
        // framesync so just skip over it here without the check
        continue;
      }
      return j;
    }

    return -1;
  }

  void readFrameHeader(const uint8_t* data) {
    assert(data[0] == 0xFF);
    assert((data[1] & 0b11100000) == 0b11100000);

    memcpy(&header, data, sizeof(header));

    LOGI("- sample rate: %u", getSampleRate());
    LOGI("- bit rate: %d", getBitRate());
  }

  enum class FrameReason {
    VALID,
    INVALID_BITRATE_FOR_VERSION,
    INVALID_SAMPLERATE_FOR_VERSION,
    INVALID_MPEG_VERSION,
    INVALID_LAYER,
    INVALID_LAYER_II_BITRATE_AND_MODE,
    INVALID_EMPHASIS,
    INVALID_CRC,
  };

  FrameReason validateFrameHeader(const FrameHeader& header) {
    if (header.AudioVersion == FrameHeader::AudioVersionID::INVALID) {
      LOGI("invalid mpeg version");
      return FrameReason::INVALID_MPEG_VERSION;
    }

    if (header.Layer == FrameHeader::LayerID::INVALID) {
      LOGI("invalid layer");
      return FrameReason::INVALID_LAYER;
    }

    if (header.getBitRate() == FrameHeader::SpecialBitrate::INVALID) {
      LOGI("invalid bitrate");
      return FrameReason::INVALID_BITRATE_FOR_VERSION;
    }

    if (header.getSampleRate() == FrameHeader::SpecialSampleRate::RESERVED) {
      LOGI("invalid samplerate");
      return FrameReason::INVALID_SAMPLERATE_FOR_VERSION;
    }

    // For Layer II there are some combinations of bitrate and mode which are
    // not allowed
    if (header.Layer == FrameHeader::LayerID::LAYER_2) {
      if (header.ChannelMode == FrameHeader::ChannelModeID::SINGLE) {
        if (header.getBitRate() >= 224000) {
          LOGI("invalid bitrate >224000");
          return FrameReason::INVALID_LAYER_II_BITRATE_AND_MODE;
        }
      } else {
        if (header.getBitRate() >= 32000 && header.getBitRate() <= 56000) {
          LOGI("invalid bitrate >32000");
          return FrameReason::INVALID_LAYER_II_BITRATE_AND_MODE;
        }

        if (header.getBitRate() == 80000) {
          LOGI("invalid bitrate >80000");
          return FrameReason::INVALID_LAYER_II_BITRATE_AND_MODE;
        }
      }
    }

    if (header.Emphasis == FrameHeader::EmphasisID::INVALID) {
      LOGI("invalid Emphasis");
      return FrameReason::INVALID_EMPHASIS;
    }

    return FrameReason::VALID;
  }
};

}  // namespace audio_tools