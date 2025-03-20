#pragma once

#define TS_PACKET_SIZE 188

#ifndef MTS_WRITE_BUFFER_SIZE
#define MTS_WRITE_BUFFER_SIZE 2000
#endif

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioToolsConfig.h"
#include "stdlib.h"

namespace audio_tools {

/**
 * @brief PMT Program Element Stream Types
 * @ingroup basic
 */
enum class MTSStreamType {
  VIDEO = 0x01,
  VIDEO_H262 = 0x02,
  AUDIO_MP3 = 0x03,
  AUDIO_MP3_LOW_BITRATE = 0x04,
  PRV_SECTIONS = 0x05,
  PES_PRV = 0x06,
  MHEG = 0x07,
  H222_0_DSM_CC = 0x08,
  H222_1 = 0x09,
  A = 0x0A,
  B = 0x0B,
  C = 0x0C,
  D = 0x0D,
  H222_0_AUX = 0x0E,
  AUDIO_AAC = 0x0F,
  VISUAL = 0x10,
  AUDIO_AAC_LATM = 0x11,
  SL_PES = 0x12,
  SL_SECTIONS = 0x13,
  SYNC_DOWNLOAD = 0x14,
  PES_METADATA = 0x15,
  METDATA_SECTIONS = 0x16,
  METADATA_DATA_CAROUSEL = 0x17,
  METADATA_OBJ_CAROUSEL = 0x18,
  METADATA_SYNC_DOWNLOAD = 0x19,
  IPMP = 0x1A,
  VIDEO_AVC = 0X1B,
  VIDEO_H222_0 = 0x1C,
  DCII_VIDEO = 0x80,
  AUDIO_A53 = 0x81,
  SCTE_STD_SUBTITLE = 0x82,
  SCTE_ISOCH_DATA = 0x83,
  ATSC_PROG_ID = 0x85,
  SCTE_25 = 0x86,
  AUDIO_EAC3 = 0x87,
  AUDIO_DTS_HD = 0x88,
  DVB_MPE_FEC = 0x90,
  ULE = 0x91,
  VEI = 0x92,
  ATSC_DATA_SERVICE_TABLE = 0x95,
  SCTE_IP_DATA = 0xA0,
  DCII_TEXT = 0xC0,
  ATSC_SYNC_DATA = 0xC2,
  SCTE_AYSNC_DATA = 0xC3,
  ATSC_USER_PRIV_PROG_ELEMENTS = 0xC4,
  VC1 = 0xEA,
  ATSC_USER_PRIV = 0xEB,
};

// enum class AACProfile : uint8_t {
//   MAIN = 0,  // AAC Main (High complexity, rarely used)
//   LC = 1,    // AAC Low Complexity (Most common)
//   SSR = 2,   // AAC Scalable Sample Rate (Rare)
//   LTP = 3    // AAC Long Term Prediction (Not widely supported)
// };

/**
 * @brief MPEG-TS (MTS) decoder. Extracts (demuxes) the indicaated audio/video
 * data from a MPEG-TS (MTS) data stream. You can define the relevant stream
 * types via the API: addStreamType(MTSStreamType). By default, the
 * decoder selects the AUDIO_AAC, AUDIO_AAC_LATM stream types.
 *
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 **/

class MTSDecoder : public AudioDecoder {
 public:
  /// Default constructor
  MTSDecoder() = default;
  /// Provide the AAC decoder (or MP3 Decoder) to receive the extracted content
  MTSDecoder(AudioDecoder &dec) { p_dec = &dec; };
  /// Start the prcessor
  bool begin() override {
    TRACED();
    pmt_pid = 0xFFFF; // undefined
    pes_count = 0;
    is_adts_missing = false;
    open_pes_data_size = 0;
    frame_length = 0;

    // default supported stream types
    if (stream_types.empty()) {
      addStreamType(MTSStreamType::AUDIO_AAC);
      addStreamType(MTSStreamType::AUDIO_AAC_LATM);
    }

    // automatically close when called multiple times
    if (is_active) {
      end();
    }

    if (p_dec) p_dec->begin();
    is_active = true;
    return true;
  }

  /// Stops the processing
  void end() override {
    TRACED();
    if (p_dec) p_dec->end();
    is_active = false;
  }

  virtual operator bool() override { return is_active; }

  /// Provides the mime type: "video/MP2T";
  const char *mime() { return "video/MP2T"; }

  size_t write(const uint8_t *data, size_t len) override {
    // only process when open
    if (!is_active) {
      TRACEE();
      return 0;
    }

    // wait until we have enough data
    if (buffer.availableForWrite() < len) {
      LOGI("MTSDecoder::write: Buffer full");
      demux();
      return 0;
    }
    LOGI("MTSDecoder::write: %d", (int)len);
    size_t result = buffer.writeArray((uint8_t *)data, len);
    demux();
    return result;
  }

  /// Set a new write buffer size (default is 2000)
  void resizeBuffer(int size) { buffer.resize(size); }

  /// Clears the stream type filter
  void clearStreamTypes() {
    TRACED();
    stream_types.clear();
  }

  /// Defines the stream type that should be extracted
  void addStreamType(MTSStreamType type) {
    TRACED();
    stream_types.push_back(type);
  }

  /// Checks if the stream type is active
  bool isStreamTypeActive(MTSStreamType type) {
    for (int j = 0; j < stream_types.size(); j++) {
      if (stream_types[j] == type) return true;
    }
    return false;
  }

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
  bool is_active = false;
  SingleBuffer<uint8_t> buffer{MTS_WRITE_BUFFER_SIZE};
  Vector<MTSStreamType> stream_types;
  Vector<int> pids{0};
  AudioDecoder *p_dec = nullptr;
  uint16_t pmt_pid = 0xFFFF;
  // AACProfile aac_profile = AACProfile::LC;
  MTSStreamType selected_stream_type;
  int open_pes_data_size = 0;
  int frame_length = 0;
  bool is_adts_missing = false;
  size_t pes_count = 0;

  /// Add the PID for which we want to extract the audio data from the PES
  /// packets
  void addPID(uint16_t pid) {
    if (pid == 0) return;
    for (int j = 0; j < pids.size(); j++) {
      if (pids[j] == pid) return;
    }
    LOGI("-> PMT PID: 0x%04X(%d)", pid, pid);
    pids.push_back(pid);
  }

  /// demux the available data
  void demux() {
    TRACED();
    int count = 0;
    while (parse()) {
      LOGI("demux: step #%d with PES #%d", ++count, (int)pes_count);
    }
    LOGI("Number of demux calls: %d", count);
  }

  /// Find the position of the next sync byte: Usually on position 0
  int syncPos() {
    int len = buffer.available();
    if (len < TS_PACKET_SIZE) return -1;
    for (int j = 0; j < len; j++) {
      if (buffer.data()[j] == 0x47) {
        return j;
      }
    }
    return -1;
  }

  /// Parse a single packet and remove the processed data
  bool parse() {
    int pos = syncPos();
    if (pos < 0) return false;
    if (pos != 0) {
      LOGW("Sync byte not found at position 0. Skipping %d bytes", pos);
      buffer.clearArray(pos);
    }
    // parse data
    uint8_t *packet = buffer.data();
    int pid = ((packet[1] & 0x1F) << 8) | (packet[2] & 0xFF);
    LOGI("PID: 0x%04X(%d)", pid, pid);

    // PES contains the audio data
    if (!is_adts_missing && pids.contains(pid)) {
      parsePES(packet, pid);
    } else {
      parsePacket(packet, pid);
    }

    // remove processed data
    buffer.clearArray(TS_PACKET_SIZE);
    return true;
  }

  /// Detailed processing for parsing a single packet
  void parsePacket(uint8_t *packet, int pid) {
    TRACEI();
    bool payloadUnitStartIndicator = false;

    int payloadStart =
        getPayloadStart(packet, false, payloadUnitStartIndicator);
    int len = TS_PACKET_SIZE - payloadStart;

    // if we are at the beginning we start with a pat
    if (pid == 0 && payloadUnitStartIndicator) {
      pids.clear();
    }

    // PID 0 is for PAT
    if (pid == 0) {
      parsePAT(&packet[payloadStart], len);
    } else if (pid == pmt_pid && packet[payloadStart] == 0x02) {
      parsePMT(&packet[payloadStart], len);
    } else {
      LOGE("-> Packet ignored for PID 0x%x", pid);
    }
  }

  int getPayloadStart(uint8_t *packet, bool isPES,
                      bool &payloadUnitStartIndicator) {
    uint8_t adaptionField = (packet[3] & 0x30) >> 4;
    int adaptationSize = 0;
    int offset = 4;  // Start after TS header (4 bytes)

    // Check for adaptation field
    // 00 (0) → Invalid (should never happen).
    // 01 (1) → Payload only (no adaptation field).
    // 10 (2) → Adaptation field only (no payload).
    // 11 (3) → Adaptation field + payload.
    if (adaptionField == 0b11) {  // Adaptation field exists
      adaptationSize = packet[4] + 1;
      offset += adaptationSize;
    }

    // If PUSI is set, there's a pointer field (skip it)
    if (packet[1] & 0x40) {
      if (!isPES) offset += packet[offset] + 1;
      payloadUnitStartIndicator = true;
    }

    LOGI("Payload Unit Start Indicator (PUSI): %d", payloadUnitStartIndicator);
    LOGI("Adaption Field Control: 0x%x / size: %d", adaptionField,
         adaptationSize);

    return offset;
  }

  void parsePAT(uint8_t *pat, int len) {
    TRACEI();
    assert(pat[0] == 0);  // Program Association section
    int startOfProgramNums = 8;
    int lengthOfPATValue = 4;
    int sectionLength = ((pat[1] & 0x0F) << 8) | (pat[2] & 0xFF);
    LOGI("PAT Section Length: %d", sectionLength);
    if (sectionLength >= len) {
      LOGE("Unexpected PAT Section Length: %d", sectionLength);
      sectionLength = len;
    }
    int indexOfPids = 0;
    for (int i = startOfProgramNums; i <= sectionLength;
         i += lengthOfPATValue) {
      int program_number = ((pat[i] & 0xFF) << 8) | (pat[i + 1] & 0xFF);
      int pid = ((pat[i + 2] & 0x1F) << 8) | (pat[i + 3] & 0xFF);
      LOGI("Program Num: 0x%04X(%d) / PID: 0x%04X(%d) ", program_number,
           program_number, pid, pid);

      if (pmt_pid == 0xFFFF && pid >= 0x0020 && pid <= 0x1FFE) {
        pmt_pid = pid;
      }
    }
    LOGI("Using PMT PID: 0x%04X(%d)", pmt_pid, pmt_pid);
  }

  void parsePMT(uint8_t *pmt, int len) {
    TRACEI();
    assert(pmt[0] == 0x02);  // Program Association section
    int staticLengthOfPMT = 12;
    int sectionLength = ((pmt[1] & 0x0F) << 8) | (pmt[2] & 0xFF);
    LOGI("- PMT Section Length: %d", sectionLength);
    int programInfoLength = ((pmt[10] & 0x0F) << 8) | (pmt[11] & 0xFF);
    LOGI("- PMT Program Info Length: %d", programInfoLength);

    int cursor = staticLengthOfPMT + programInfoLength;
    while (cursor < sectionLength - 1) {
      MTSStreamType streamType = static_cast<MTSStreamType>(pmt[cursor] & 0xFF);
      int elementaryPID =
          ((pmt[cursor + 1] & 0x1F) << 8) | (pmt[cursor + 2] & 0xFF);
      LOGI("-- Stream Type: 0x%02X(%d) [%s] for Elementary PID: 0x%04X(%d)",
           (int)streamType, (int)streamType, toStr(streamType), elementaryPID,
           elementaryPID);

      if (isStreamTypeActive(streamType)) {
        selected_stream_type = streamType;
        addPID(elementaryPID);
      }

      int esInfoLength =
          ((pmt[cursor + 3] & 0x0F) << 8) | (pmt[cursor + 4] & 0xFF);
      LOGI("-- ES Info Length: 0x%04X(%d)", esInfoLength, esInfoLength);
      cursor += 5 + esInfoLength;
    }
  }

  void parsePES(uint8_t *packet, int pid) {
    LOGI("parsePES: %d", pid);
    ++pes_count;

    // calculate payload start
    bool payloadUnitStartIndicator = false;
    int payloadStart = getPayloadStart(packet, true, payloadUnitStartIndicator);

    // PES
    uint8_t *pes = packet + payloadStart;
    int len = TS_PACKET_SIZE - payloadStart;
    // PES (AAC) data
    uint8_t *pesData = nullptr;
    int pesDataSize = 0;

    if (payloadUnitStartIndicator) {
      assert(len >= 6);
      // PES header is not alligned correctly
      if (!isPESStartCodeValid(pes)) {
        LOGE("PES header not aligned correctly");
        return;
      }

      int pesPacketLength =
          (static_cast<int>(pes[4]) << 8) | static_cast<int>(pes[5]);

      // PES Header size is at least 6 bytes, but can be larger with optional
      // fields
      int pesHeaderSize = 6;
      if ((pes[6] & 0xC0) != 0) {  // Check for PTS/DTS flags
        pesHeaderSize += 3 + ((pes[7] & 0xC0) == 0xC0 ? 5 : 0);
        pesHeaderSize += pes[8];  // PES header stuffing size
      }
      LOGI("- PES Header Size: %d", pesHeaderSize);
      pesData = pes + pesHeaderSize;
      pesDataSize = len - pesHeaderSize;

      assert(pesHeaderSize < len);
      assert(pesDataSize > 0);

      /// Check for ADTS
      if (pes_count == 1 && selected_stream_type == MTSStreamType::AUDIO_AAC) {
        is_adts_missing = findSyncWord(pesData, pesDataSize) == -1;
      }

      open_pes_data_size = pesPacketLength;

    } else {
      pesData = pes;
      pesDataSize = len;
    }

    // Recalculate the open data
    open_pes_data_size -= pesDataSize;
    if (open_pes_data_size < 0) {
      return;
    }

    /// Write the data
    LOGI("- writing %d bytes (open: %d)", pesDataSize, open_pes_data_size);
    if (p_print) {
      size_t result = writeData<uint8_t>(p_print, pesData, pesDataSize);
      assert(result == pesDataSize);
    }
    if (p_dec) {
      size_t result =
          writeDataT<uint8_t, AudioDecoder>(p_dec, pesData, pesDataSize);
      assert(result == pesDataSize);
    }
  }

  /// check for PES packet start code prefix
  bool isPESStartCodeValid(uint8_t *pes) {
    if (pes[0] != 0) return false;
    if (pes[1] != 0) return false;
    if (pes[2] != 0x1) return false;
    return true;
  }

  /// Convert the relevant MTSStreamType to a string
  const char *toStr(MTSStreamType type) {
    switch (type) {
      case MTSStreamType::AUDIO_MP3:
        return "AUDIO_MP3";
      case MTSStreamType::AUDIO_MP3_LOW_BITRATE:
        return "AUDIO_MP3_LOW_BITRATE";
      case MTSStreamType::AUDIO_AAC:
        return "AUDIO_AAC";
      case MTSStreamType::AUDIO_AAC_LATM:
        return "AUDIO_AAC_LATM";
      default:
        return "UNKNOWN";
    }
  }

  /// Finds the mp3/aac sync word
  int findSyncWord(const uint8_t *buf, size_t nBytes, uint8_t synch = 0xFF,
                   uint8_t syncl = 0xF0) {
    for (int i = 0; i < nBytes - 1; i++) {
      if ((buf[i + 0] & synch) == synch && (buf[i + 1] & syncl) == syncl)
        return i;
    }
    return -1;
  }
};

using MPEG_TSDecoder = MTSDecoder;

}  // namespace audio_tools
