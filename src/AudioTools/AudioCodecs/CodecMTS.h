#pragma once

#define TS_PACKET_SIZE 188

#ifndef MTS_WRITE_BUFFER_SIZE
#define MTS_WRITE_BUFFER_SIZE 2000
#endif

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "stdlib.h"

namespace audio_tools {

/**
 * PMT Program Element Stream Type
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

/**
 * @brief MPEG-TS (MTS) decoder. Extracts (demuxes) the AAC audio data from a
 *MPEG-TS (MTS) data stream. You can define the relevant stream types via the
 *API: For details see https://tsduck.io/download/docs/mpegts-introduction.pdf
 *
 * Status: experimental!
 *
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 **/

class MTSDecoder : public AudioDecoder {
 public:
  MTSDecoder() = default;
  MTSDecoder(AudioDecoder &dec) { p_dec = &dec; };

  bool begin() override {
    TRACED();
    if (p_dec) p_dec->begin();
    pmt_pid = 0xFFFF;

    // default supported stream types
    if (stream_types.empty()) {
      addStreamType(MTSStreamType::AUDIO_AAC);
      addStreamType(MTSStreamType::AUDIO_AAC_LATM);
    }

    // automatically close when called multiple times
    if (is_active) {
      end();
    }

    is_active = true;
    return true;
  }

  void end() override {
    TRACED();
    if (p_dec) p_dec->end();
    is_active = false;
  }

  virtual operator bool() override { return is_active; }

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
      return 0;
    }
    LOGI("MTSDecoder::write: %d", (int)len);
    size_t result = buffer.writeArray((uint8_t *)data, len);
    // parse the data
    demux();
    return result;
  }

  void flush() {}

  /// Set a new write buffer size (default is 2000)
  void resizeBuffer(int size) { buffer.resize(size); }

  void clearStreamTypes() {
    TRACED();
    stream_types.clear();
  }

  void addStreamType(MTSStreamType type) {
    TRACED();
    stream_types.push_back(type);
  }

  bool isStreamTypeActive(MTSStreamType type) {
    for (int j = 0; j < stream_types.size(); j++) {
      if (stream_types[j] == type) return true;
    }
    return false;
  }

 protected:
  bool is_active = false;
  SingleBuffer<uint8_t> buffer{MTS_WRITE_BUFFER_SIZE};
  Vector<MTSStreamType> stream_types;
  Vector<int> pids{0};
  AudioDecoder *p_dec = nullptr;
  uint16_t pmt_pid = 0xFFFF;

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
      LOGI("demux: # %d", ++count);
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
    // parse data
    parsePacket();
    // assert(pos == 0);
    // remove processed data
    buffer.clearArray(pos == 0 ? TS_PACKET_SIZE : pos);
    return true;
  }

  /// Detailed processing for parsing a single packet
  void parsePacket() {
    TRACEI();
    uint8_t *packet = buffer.data();
    int pid = ((packet[1] & 0x1F) << 8) | (packet[2] & 0xFF);
    LOGI("PID: 0x%04X(%d)", pid, pid);
    int payloadUnitStartIndicator = (packet[1] & 0x40) >> 6;
    LOGI("Payload Unit Start Indicator: %d", payloadUnitStartIndicator);
    int adaptionFieldControl = (packet[3] & 0x30) == 0x20;
    LOGI("Adaption Field Control: %d", adaptionFieldControl);

    bool has_payload = true;
    if ((packet[3] & 0x10) >> 4 == 0) {
      has_payload = false;
    }

    // Locate start of PSI section (skip adaptation field if present)
    int payload_offset = 4;
    if ((packet[3] & 0x20) >> 5 == 1) {  // Adaptation field exists
      payload_offset += packet[4] + 1;
    }

    // Adjust for the Pointer Field (only if PUSI is set)
    if (payloadUnitStartIndicator) {
      payload_offset += packet[payload_offset] + 1;
    }

    // if we are at the beginning we start with a pat
    if (pid == 0 && payloadUnitStartIndicator) {
      pids.clear();
    }

    int payloadStart = payload_offset;
    int len = TS_PACKET_SIZE - payloadStart;

    // PID 0 is for PAT
    if (pid == 0) {
      parsePAT(&packet[payloadStart], len);
    } else if (pid == pmt_pid && packet[payloadStart] == 0x02) {
      parsePMT(&packet[payloadStart], len);
    } else if (pids.contains(pid)) {
      parsePES(&packet[payloadStart], payloadUnitStartIndicator ? true : false,
               len);
    } else {
      LOGE("-> Packet ignored for %d", pid);
    }
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

  void parsePMT(uint8_t *pat, int len) {
    TRACEI();
    assert(pat[0] == 0x02);  // Program Association section
    int staticLengthOfPMT = 12;
    int sectionLength = ((pat[1] & 0x0F) << 8) | (pat[2] & 0xFF);
    LOGI("PMT Section Length: %d", sectionLength);
    int programInfoLength = ((pat[10] & 0x0F) << 8) | (pat[11] & 0xFF);
    LOGI("PMT Program Info Length: %d", programInfoLength);

    int cursor = staticLengthOfPMT + programInfoLength;
    while (cursor < sectionLength - 1) {
      int streamType = pat[cursor] & 0xFF;
      int elementaryPID =
          ((pat[cursor + 1] & 0x1F) << 8) | (pat[cursor + 2] & 0xFF);
      LOGI("Stream Type: 0x%02X(%d) Elementary PID: 0x%04X(%d)", streamType,
           streamType, elementaryPID, elementaryPID);

      if (isStreamTypeActive((MTSStreamType)streamType)) {
        addPID(elementaryPID);
      }

      int esInfoLength =
          ((pat[cursor + 3] & 0x0F) << 8) | (pat[cursor + 4] & 0xFF);
      LOGI("ES Info Length: 0x%04X(%d)", esInfoLength, esInfoLength);
      cursor += 5 + esInfoLength;
    }
  }

  void parsePES(uint8_t *pat, const bool isNewPayload, int len) {
    TRACEI();
    int dataSize = 0;
    if (isNewPayload) {
      uint8_t streamID = pat[3] & 0xFF;
      LOGI("Stream ID:%02X ", streamID);
      const uint8_t posOfPacketLengthLatterHalf = 5;
      int pesRemainingPacketLength = ((pat[4] & 0xFF) << 8) | (pat[5] & 0xFF);
      LOGI("PES Packet length: %d", pesRemainingPacketLength);
      // pesDataLength = pesRemainingPacketLength;
      const uint8_t posOfHeaderLength = 8;
      uint8_t pesRemainingHeaderLength = pat[posOfHeaderLength] & 0xFF;
      LOGI("PES Header length: %d", pesRemainingHeaderLength);
      int startOfData = pesRemainingHeaderLength;
      dataSize = len - startOfData;
      LOGI("PES Data size: %d", dataSize);
      if (dataSize < 0) dataSize = 0;
      if (p_print) p_print->write(&pat[startOfData], dataSize);
      if (p_dec) p_dec->write(&pat[startOfData], dataSize);
      // pesDataLength -= len - (posOfPacketLengthLatterHalf + 1);
    } else {
      dataSize = len;
      LOGI("PES Data size: %d", dataSize);
      if (p_print) p_print->write(pat, dataSize);
      if (p_dec) p_dec->write(pat, dataSize);
      // pesDataLength -= dataSize;
    }
  }
};

using MPEG_TSDecoder = MTSDecoder;

}  // namespace audio_tools
