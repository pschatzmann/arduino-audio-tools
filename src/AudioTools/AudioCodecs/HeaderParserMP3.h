#pragma once
#include "AudioTools/CoreAudio/AudioBasic/StrView.h"

namespace audio_tools {

/**
 * @brief  MP3 header parser that processes MP3 data incrementally and 
 * extracts complete MP3 frames. Can validate MP3 data and extract audio 
 * information. When used with a Print output, it splits incoming data into 
 * complete MP3 frames and writes them to the output stream.
 * 
 * Features:
 * - Incremental processing of MP3 data in small chunks
 * - Frame synchronization and validation
 * - Extraction of audio information (sample rate, bit rate, etc.)
 * - Output of complete MP3 frames only
 * - Support for all MPEG versions (1, 2, 2.5) and layers
 * 
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class HeaderParserMP3 {
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
      signed char rate_byte = rateTable[(int)AudioVersion][(int)Layer][(int)BitrateIndex];
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

      return rateTable[(int)AudioVersion][(int)SampleRateIndex];
    }

    int getFrameLength() {
      int sample_rate = getSampleRate();
      if (sample_rate == 0) return 0;
      int value = (AudioVersion == FrameHeader::AudioVersionID::MPEG_1) ? 144 : 72;
      return int((value * getBitRate() / sample_rate) + Padding);
    }
  };

 public:
  /// Default constructor
  HeaderParserMP3() = default;

  /// Constructor for write support
  HeaderParserMP3(Print& output, int bufferSize=2048) : p_output(&output), buffer_size(bufferSize) {}

  /// split up the data into mp3 segements and write to output
  size_t write(const uint8_t* data, size_t len) {
    if (buffer.size() < buffer_size) buffer.resize(buffer_size);

    for (int i = 0; i < len; i++) {
      buffer.write(data[i]);
      if (buffer.isFull()) {
        while (processBuffer()) ;
      }
    }
    
    return len;
  }

  void flush() {
    if (p_output == nullptr) return;
    while (processBuffer()) ;
  }

  /// Returns true if a valid frame has been detected
  bool isValid() { return last_frame_size > 0;}

  /// parses the header string and returns true if this is a valid mp3 file
  bool isValid(const uint8_t* data, int len) {
    if (data == nullptr || len < 10) {
      LOGE("Invalid input data or too small");
      return false;
    }

    memset(&header, 0, sizeof(header));
    int valid_frames_found = 0;
    int consecutive_frames = 0;
    const int MIN_FRAMES_TO_VALIDATE = 3; // Require at least 3 consecutive valid frames
    const int MAX_SEARCH_DISTANCE = 8192; // Limit search to prevent endless loops
    
    // Check for ID3v2 tag at beginning
    if (len >= 10 && memcmp(data, "ID3", 3) == 0) {
      LOGI("ID3v2 tag found");
      // Skip ID3v2 tag to find actual audio data
      int id3_size = ((data[6] & 0x7F) << 21) | ((data[7] & 0x7F) << 14) | 
                     ((data[8] & 0x7F) << 7) | (data[9] & 0x7F);
      int audio_start = 10 + id3_size;
      if (audio_start < len) {
        return isValid(data + audio_start, len - audio_start);
      }
      return true; // Valid ID3 tag, assume MP3
    }

    // Look for first frame sync
    int sync_pos = seekFrameSync(data, min(len, MAX_SEARCH_DISTANCE));
    if (sync_pos == -1) {
      LOGE("No frame sync found in first %d bytes", MAX_SEARCH_DISTANCE);
      return false;
    }

    // Quick check for VBR headers (Xing/Info/VBRI)
    if (contains(data + sync_pos, "Xing", len - sync_pos) ||
        contains(data + sync_pos, "Info", len - sync_pos) ||
        contains(data + sync_pos, "VBRI", len - sync_pos)) {
      LOGI("VBR header found (Xing/Info/VBRI)");
      return true;
    }

    // Validate multiple consecutive frames for higher confidence
    int current_pos = sync_pos;
    FrameHeader first_header;
    bool first_header_set = false;
    
    while (current_pos < len && (current_pos - sync_pos) < MAX_SEARCH_DISTANCE) {
      int len_available = len - current_pos;
      
      // Need at least header size
      if (len_available < sizeof(FrameHeader)) {
        LOGD("Not enough data for header at position %d", current_pos);
        break;
      }

      // Read and validate frame header
      FrameHeader temp_header;
      memcpy(&temp_header, data + current_pos, sizeof(temp_header));
      
      if (validateFrameHeader(temp_header) != FrameReason::VALID) {
        LOGD("Invalid frame header at position %d", current_pos);
        consecutive_frames = 0;
        // Look for next sync
        current_pos = seekFrameSync(data + current_pos + 1, len - current_pos - 1);
        if (current_pos == -1) break;
        current_pos += current_pos + 1; // Adjust for offset
        continue;
      }

      // Calculate frame length
      int frame_len = temp_header.getFrameLength();
      if (frame_len <= 0 || frame_len > 4096) {
        LOGD("Invalid frame length %d at position %d", frame_len, current_pos);
        consecutive_frames = 0;
        current_pos++;
        continue;
      }

      // For first valid frame, store header for consistency checking
      if (!first_header_set) {
        first_header = temp_header;
        first_header_set = true;
        header = temp_header; // Store for external access
        
        // For small buffers, do additional single-frame validation
        if (len < 1024) {
          // Verify this looks like a reasonable MP3 frame
          if (temp_header.getSampleRate() == 0 || temp_header.getBitRate() <= 0) {
            LOGD("Invalid audio parameters in frame at position %d", current_pos);
            first_header_set = false;
            consecutive_frames = 0;
            current_pos++;
            continue;
          }
          
          // Check if frame length is reasonable for the given bitrate
          int expected_frame_size = (temp_header.AudioVersion == FrameHeader::AudioVersionID::MPEG_1) ? 
                                   (144 * temp_header.getBitRate() / temp_header.getSampleRate()) :
                                   (72 * temp_header.getBitRate() / temp_header.getSampleRate());
          if (abs(frame_len - expected_frame_size) > expected_frame_size * 0.1) { // Allow 10% variance
            LOGD("Frame length %d doesn't match expected %d for bitrate", frame_len, expected_frame_size);
            first_header_set = false;
            consecutive_frames = 0;
            current_pos++;
            continue;
          }
        }
      } else {
        // Check consistency with first frame (sample rate, version, layer should match in CBR)
        if (temp_header.AudioVersion != first_header.AudioVersion ||
            temp_header.Layer != first_header.Layer ||
            temp_header.getSampleRate() != first_header.getSampleRate()) {
          LOGD("Frame parameters inconsistent at position %d", current_pos);
          // This might be VBR, but continue validation
        }
      }

      valid_frames_found++;
      consecutive_frames++;
      
      // Check if we have enough data for the complete frame
      if (len_available < frame_len) {
        LOGD("Incomplete frame at position %d (need %d, have %d)", 
             current_pos, frame_len, len_available);
        break;
      }

      // Look for next frame sync at expected position
      int next_pos = current_pos + frame_len;
      if (next_pos + 1 < len) {
        if (seekFrameSync(data + next_pos, min(4, len - next_pos)) == 0) {
          // Found sync at expected position
          current_pos = next_pos;
          continue;
        } else {
          LOGD("No sync at expected position %d", next_pos);
          consecutive_frames = 0;
        }
      } else {
        // End of data reached
        break;
      }

      // If we lost sync, search for next frame
      int next_sync = seekFrameSync(data + current_pos + 1, len - current_pos - 1);
      if (next_sync == -1) break;
      current_pos = current_pos + 1 + next_sync;
    }

    // Adaptive validation criteria based on available data
    bool is_valid_mp3 = false;
    
    if (len >= 2048) {
      // For larger buffers, require strict consecutive frame validation
      is_valid_mp3 = (consecutive_frames >= MIN_FRAMES_TO_VALIDATE);
    } else if (len >= 1024) {
      // For 1KB+ buffers, require at least 2 consecutive frames OR 3 total valid frames
      is_valid_mp3 = (consecutive_frames >= 2) || (valid_frames_found >= MIN_FRAMES_TO_VALIDATE);
    } else {
      // For smaller buffers, be more lenient - 1 good frame with proper validation
      is_valid_mp3 = (valid_frames_found >= 1) && first_header_set;
    }
    
    if (is_valid_mp3 && first_header_set) {
      LOGI("-------------------");
      LOGI("MP3 validation: VALID");
      LOGI("Data size: %d bytes", len);
      LOGI("Valid frames found: %d", valid_frames_found);
      LOGI("Consecutive frames: %d", consecutive_frames);
      if (len >= 2048) {
        LOGI("Validation mode: STRICT (large buffer)");
      } else if (len >= 1024) {
        LOGI("Validation mode: MODERATE (1KB+ buffer)");
      } else {
        LOGI("Validation mode: LENIENT (small buffer)");
      }
      LOGI("Frame size: %d", getFrameLength());
      LOGI("Sample rate: %u", getSampleRate());
      LOGI("Bit rate: %d", getBitRate());
      LOGI("Padding: %d", getFrameHeader().Padding);
      LOGI("Layer: %s (0x%x)", getLayerStr(), (int)getFrameHeader().Layer);
      LOGI("Version: %s (0x%x)", getVersionStr(), (int)getFrameHeader().AudioVersion);
      LOGI("-------------------");
    } else {
      LOGI("MP3 validation: INVALID (frames: %d, consecutive: %d, size: %d)", 
           valid_frames_found, consecutive_frames, len);
    }
    
    return is_valid_mp3;
  }

  /// Sample rate from mp3 header
  uint16_t getSampleRate() const { 
    return frame_header_valid ? header.getSampleRate() : 0; 
  }

  /// Bit rate from mp3 header
  int getBitRate() const { 
    return frame_header_valid ? header.getBitRate() : 0; 
  }

  /// Frame length from mp3 header
  int getFrameLength() { 
    return frame_header_valid ? header.getFrameLength() : 0; 
  }

  /// Provides the estimated playing time in seconds based on the bitrate of the
  /// first segment
  size_t getPlayingTime(size_t fileSizeBytes) {
    int bitrate = getBitRate();
    if (bitrate == 0) return 0;
    return fileSizeBytes / bitrate;
  }

  /// Provides a string representation of the MPEG version
  const char* getVersionStr() const {
    return header.AudioVersion == FrameHeader::AudioVersionID::MPEG_1   ? "1"
           : header.AudioVersion == FrameHeader::AudioVersionID::MPEG_2 ? "2"
           : header.AudioVersion == FrameHeader::AudioVersionID::MPEG_2_5
               ? "2.5"
               : "INVALID";
  }

  /// Provides a string representation of the MPEG layer
  const char* getLayerStr() const {
    return header.Layer == FrameHeader::LayerID::LAYER_1   ? "1"
           : header.Layer == FrameHeader::LayerID::LAYER_2 ? "2"
           : header.Layer == FrameHeader::LayerID::LAYER_3 ? "3"
                                                           : "INVALID";
  }

  /// number of samples per mp3 frame
  int getSamplesPerFrame() {
    if (header.Layer != FrameHeader::LayerID::LAYER_3) return 0;
    // samples for layer 3 are fixed
    return header.AudioVersion == FrameHeader::AudioVersionID::MPEG_1   ? 1152 : 576;
  }

  /// playing time per frame in ms
  size_t getTimePerFrameMs() {
    int sample_rate = getSampleRate();
    if (sample_rate == 0) return 0;
    return (1000 * getSamplesPerFrame()) / sample_rate;
  }

  /// frame rate in Hz (frames per second)
  size_t getFrameRateHz() {
    int time_per_frame = getTimePerFrameMs();
    if (time_per_frame == 0) return 0;
    return 1000 / time_per_frame;
  }

  // provides the parsed MP3 frame header
  FrameHeader getFrameHeader() { 
    return frame_header_valid ? header : FrameHeader{}; 
  }

  /// Returns true if we have parsed at least one valid frame
  bool hasValidFrame() const {
    return frame_header_valid;
  }

  /// Clears internal buffer and resets state
  void reset() {
    buffer.reset();
    frame_header_valid = false;
    memset(&header, 0, sizeof(header));
  }

  /// Finds the mp3/aac sync word
  int findSyncWord(const uint8_t* buf, size_t nBytes, uint8_t synch = 0xFF,
                   uint8_t syncl = 0xF0) {
    for (int i = 0; i < nBytes - 1; i++) {
      if ((buf[i + 0] & synch) == synch && (buf[i + 1] & syncl) == syncl)
        return i;
    }
    return -1;
  }

 protected:
  FrameHeader header;
  Print* p_output = nullptr;
  SingleBuffer<uint8_t> buffer{0}; // Max MP3 frame ~4KB + reserves
  bool frame_header_valid = false;
  size_t buffer_size = 0;
  size_t last_frame_size = 0;

  /// Processes the internal buffer to extract complete mp3 frames
  bool processBuffer() {
    bool progress = false;
    
    while (buffer.available() >= 4) { // Need at least 4 bytes for header
      
      // Get direct access to buffer data
      size_t available = buffer.available();
      uint8_t* temp_data = buffer.data();
      
      // Find frame sync
      int sync_pos = seekFrameSync(temp_data, available);
      if (sync_pos == -1) {
        // No sync found, keep last few bytes in case sync spans buffer boundary
        size_t to_remove = (available > 3) ? available - 3 : 0;
        if (to_remove > 0) {
          buffer.clearArray(to_remove);
        }
        break;
      }
      
      // Remove any data before sync
      if (sync_pos > 0) {
        buffer.clearArray(sync_pos);
        progress = true;
        continue; // Check again from new position
      }
      
      // We have sync at position 0, try to read header
      if (available < sizeof(FrameHeader)) {
        break; // Need more data for complete header
      }
      
      // Read and validate frame header
      FrameHeader temp_header;
      memcpy(&temp_header, temp_data, sizeof(temp_header));
      
      if (validateFrameHeader(temp_header) != FrameReason::VALID) {
        // Invalid header, skip this sync and look for next
        buffer.clearArray(1);
        progress = true;
        continue;
      }
      
      // Calculate frame length
      int frame_len = temp_header.getFrameLength();
      if (frame_len <= 0 || frame_len > buffer_size) { // Sanity check on frame size
        // Invalid frame length, skip this sync
        buffer.clearArray(1);
        progress = true;
        continue;
      }
      
      // Check if we have complete frame
      if (available < frame_len) {
        break; // Need more data for complete frame
      }
      
      // Verify next frame sync if we have enough data
      if (available >= frame_len + 2) {
        if (seekFrameSync(temp_data + frame_len, 2) != 0) {
          // No sync at expected position, this might not be a valid frame
          buffer.clearArray(1);
          progress = true;
          continue;
        }
      }
      
      // We have a complete valid frame, write it to output
      if (p_output != nullptr) {
        size_t written = p_output->write(temp_data, frame_len);
        if (written != frame_len) {
          // Output error, we still need to remove the frame from buffer
          LOGE("Failed to write complete frame");
        }
      }
      
      // Update header for external access
      last_frame_size = frame_len;
      header = temp_header;
      frame_header_valid = true;
      
      // Remove processed frame from buffer
      buffer.clearArray(frame_len);
      
      progress = true;
    }
    
    return progress;
  }

  bool validate(const uint8_t* data, size_t len) {
    assert(header.FrameSyncByte = 0xFF);
    // check end of frame: it must contains a sync word
    return FrameReason::VALID == validateFrameHeader(header);
  }

  bool contains(const uint8_t* data, const char* toFind, size_t len) {
    if (data == nullptr || len == 0) return false;
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

      // Check for MP3 sync: 0xFFE0 (MP3) vs 0xFFF0 (AAC ADTS)
      if ((str[j + 1] & 0b11100000) != 0b11100000) {
        // if the next byte does not have its 3 most significant bits set it is
        // not the end of the framesync and it also cannot be the start of a
        // framesync so just skip over it here without the check
        continue;
      }
      
      // Explicitly reject AAC ADTS sync patterns (0xFFF0-0xFFFF)
      // AAC ADTS requires 12 sync bits (0xFFF), while MP3 only requires 11 (0xFFE)
      if ((str[j + 1] & 0b11110000) == 0b11110000) {
        LOGD("Rejecting AAC ADTS sync pattern at position %d", j);
        continue; // This looks like AAC ADTS, skip it
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