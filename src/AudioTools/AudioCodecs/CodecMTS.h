#pragma once
#pragma once

#define TS_PACKET_SIZE 188
#define TS_HEADER_SIZE 4
#define PID_ARRAY_LEN 4

#ifndef MTS_UNDERFLOW_LIMIT
#  define MTS_UNDERFLOW_LIMIT 200
#endif

#ifndef MTS_WRITE_BUFFER_SIZE
#  define MTS_WRITE_BUFFER_SIZE 2000
#endif

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "tsdemux.h"
#include "stdlib.h"

namespace audio_tools {

/**
 * PMT Program Element Stream Type
 */
enum MTSStreamType {
    STREAM_TYPE_VIDEO                        = 0x01,
    STREAM_TYPE_VIDEO_H262                   = 0x02,
    STREAM_TYPE_AUDIO_11172                  = 0x03,
    STREAM_TYPE_AUDIO_13818_3                = 0x04,
    STREAM_TYPE_PRV_SECTIONS                 = 0x05,
    STREAM_TYPE_PES_PRV                      = 0x06,
    STREAM_TYPE_MHEG                         = 0x07,
    STREAM_TYPE_H222_0_DSM_CC                = 0x08,
    STREAM_TYPE_H222_1                       = 0x09,
    STREAM_TYPE_A                            = 0x0A,
    STREAM_TYPE_B                            = 0x0B,
    STREAM_TYPE_C                            = 0x0C,
    STREAM_TYPE_D                            = 0x0D,
    STREAM_TYPE_H222_0_AUX                   = 0x0E,
    STREAM_TYPE_AUDIO_AAC                    = 0x0F,
    STREAM_TYPE_VISUAL                       = 0x10,
    STREAM_TYPE_AUDIO_LATM                   = 0x11,
    STREAM_TYPE_SL_PES                       = 0x12,
    STREAM_TYPE_SL_SECTIONS                  = 0x13,
    STREAM_TYPE_SYNC_DOWNLOAD                = 0x14,
    STREAM_TYPE_PES_METADATA                 = 0x15,
    STREAM_TYPE_METDATA_SECTIONS             = 0x16,
    STREAM_TYPE_METADATA_DATA_CAROUSEL       = 0x17,
    STREAM_TYPE_METADATA_OBJ_CAROUSEL        = 0x18,
    STREAM_TYPE_METADATA_SYNC_DOWNLOAD       = 0x19,
    STREAM_TYPE_IPMP                         = 0x1A,
    STREAM_TYPE_VIDEO_AVC                    = 0X1B,
    STREAM_TYPE_VIDEO_H222_0                 = 0x1C,
    STREAM_TYPE_DCII_VIDEO                   = 0x80,
    STREAM_TYPE_AUDIO_A53                    = 0x81,
    STREAM_TYPE_SCTE_STD_SUBTITLE            = 0x82,
    STREAM_TYPE_SCTE_ISOCH_DATA              = 0x83,
    STREAM_TYPE_ATSC_PROG_ID                 = 0x85,
    STREAM_TYPE_SCTE_25                      = 0x86,
    STREAM_TYPE_AUDIO_EAC3                   = 0x87,
    STREAM_TYPE_AUDIO_DTS_HD                 = 0x88,
    STREAM_TYPE_DVB_MPE_FEC                  = 0x90,
    STREAM_TYPE_ULE                          = 0x91,
    STREAM_TYPE_VEI                          = 0x92,
    STREAM_TYPE_ATSC_DATA_SERVICE_TABLE      = 0x95,
    STREAM_TYPE_SCTE_IP_DATA                 = 0xA0,
    STREAM_TYPE_DCII_TEXT                    = 0xC0,
    STREAM_TYPE_ATSC_SYNC_DATA               = 0xC2,
    STREAM_TYPE_SCTE_AYSNC_DATA              = 0xC3,
    STREAM_TYPE_ATSC_USER_PRIV_PROG_ELEMENTS = 0xC4,
    STREAM_TYPE_VC1                          = 0xEA,
    STREAM_TYPE_ATSC_USER_PRIV               = 0xEB,
};

/**
 * @brief MPEG-TS (MTS) decoder. Extracts the AAC audio data from a MPEG-TS (MTS) data stream. You can
 * define the relevant stream types via the API.
 * The parsing logic was taken from: https://github.com/Yokohama-Miyazawa/M5HLSPlayer/blob/main/src/AudioGeneratorTS.cpp
 * Status: experimental!
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class MTSDecoder : public AudioDecoder {
 public:
  MTSDecoder() = default;

  bool begin() override {
    TRACED();

    // default supported stream types
    if (stream_types.empty()){
      //addStreamType(STREAM_TYPE_PES_METADATA);
      addStreamType(STREAM_TYPE_AUDIO_AAC);
      addStreamType(STREAM_TYPE_AUDIO_13818_3);
      addStreamType(STREAM_TYPE_AUDIO_LATM);
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
    is_active = false;
  }

  virtual operator bool() override { return is_active; }

  const char *mime() { return "video/MP2T"; }

  size_t write(const uint8_t *data, size_t len) override {
    if (!is_active) return 0;
    LOGD("MTSDecoder::write: %d", (int)len);
    size_t result = buffer.writeArray((uint8_t*)data, len);
    // demux
    demux(underflowLimit);
    return result;
  }

  void flush(){
    demux(0);
  }

  /// Set a new write buffer size (default is 2000)
  void resizeBuffer(int size){
    buffer.resize(size);
  }

  void clearStreamTypes(){
    TRACED();
    stream_types.clear();
  }

  void addStreamType(MTSStreamType type){
    TRACED();
    stream_types.push_back(type);
  }

  bool isStreamTypeActive(MTSStreamType type){
    for (int j=0;j<stream_types.size();j++){
      if (stream_types[j]==type) return true;
    }
    return false;
  }

 protected:
  int underflowLimit = MTS_UNDERFLOW_LIMIT;
  bool is_active = false;
  SingleBuffer<uint8_t> buffer{MTS_WRITE_BUFFER_SIZE};
  Vector<uint8_t> data{TS_PACKET_SIZE};
  Vector<MTSStreamType> stream_types;
  

  struct pid_array{
    int number;
    int pids[PID_ARRAY_LEN];
  };

  uint8_t packetBuff[TS_PACKET_SIZE];
  bool isSyncByteFound = false;
  pid_array pidsOfPMT;
  int16_t relevantPID = -1;
  int16_t pesDataLength = -1;


  void demux(int limit){
      TRACED();
      TSDCode res = TSD_OK;
      int count = 0;
      while (res == TSD_OK && buffer.available() > limit) {
        parse();
        count++;
      }
      LOGD("Number of demux calls: %d", count);
  }

  uint32_t parse() {
      int len = buffer.available();
      // If len is too short, return 0
      if(len < (TS_PACKET_SIZE - TS_HEADER_SIZE)) { return 0; }
      int read;
      int aacRead = 0;
      do {
        if(!isSyncByteFound) {
          uint8_t oneByte;
          do {
            if(!buffer.readArray(&oneByte, 1)) return 0;
          } while (oneByte != 0x47);
          isSyncByteFound = true;
          packetBuff[0]=0x47;
          read = buffer.readArray(&packetBuff[1], TS_PACKET_SIZE-1);
        } else {
          read = buffer.readArray(packetBuff, TS_PACKET_SIZE);
        }
        if(read){ 
          int len = parsePacket(packetBuff, &data[aacRead]);
          if (len<0) {
            is_active = false;
            return 0;
          }
          aacRead += len; 
        }
      } while ((len - aacRead) >= (TS_PACKET_SIZE - TS_HEADER_SIZE) && read);

      return aacRead;
    }

     int parsePacket(uint8_t *packet, uint8_t *data){
        int read = 0;

        int pid = ((packet[1] & 0x1F) << 8) | (packet[2] & 0xFF);
        LOGD("PID: 0x%04X(%d)", pid, pid);
        int payloadUnitStartIndicator = (packet[1] & 0x40) >> 6;
        LOGD("Payload Unit Start Indicator: %d", payloadUnitStartIndicator);
        int adaptionFieldControl = (packet[3] & 0x30) >> 4;
        LOGD("Adaption Field Control: %d", adaptionFieldControl);
        int remainingAdaptationFieldLength = -1;
        if ((adaptionFieldControl & 0b10) == 0b10){
            remainingAdaptationFieldLength = packet[4] & 0xFF;
            LOGD("Adaptation Field Length: %d", remainingAdaptationFieldLength);
        }
        // if we are at the beginning we start with a pat
        if (payloadUnitStartIndicator){
          pid = 0;
          pidsOfPMT.number = 0;
        }

        int payloadStart = payloadUnitStartIndicator ? 5 : 4;

        if (pid == 0){
            parsePAT(&packet[payloadStart]);
        } else if (pid == relevantPID){
            int posOfPacketStart = 4;
            if (remainingAdaptationFieldLength >= 0) posOfPacketStart = 5 + remainingAdaptationFieldLength;
            read = parsePES(&packet[posOfPacketStart], posOfPacketStart, payloadUnitStartIndicator ? true : false);
        } else if (pidsOfPMT.number){
            for (int i = 0; i < pidsOfPMT.number; i++){
              if (pid == pidsOfPMT.pids[i]){
                  parsePMT(&packet[payloadStart]);
              }
            }
        }

        return read;
    }   
  
    int parsePAT(uint8_t *pat){
        int startOfProgramNums = 8;
        int lengthOfPATValue = 4;
        int sectionLength = ((pat[1] & 0x0F) << 8) | (pat[2] & 0xFF);
        LOGD("Section Length: %d", sectionLength);
        if(sectionLength >= PID_ARRAY_LEN*lengthOfPATValue) {
          LOGE("Section Length: %d", sectionLength);
          return 0;
        }
        int indexOfPids = 0;
        for (int i = startOfProgramNums; i <= sectionLength; i += lengthOfPATValue){
            //int program_number = ((pat[i] & 0xFF) << 8) | (pat[i + 1] & 0xFF);
            //LOGD("Program Num: 0x%04X(%d)", program_number, program_number);
            int program_map_PID = ((pat[i + 2] & 0x1F) << 8) | (pat[i + 3] & 0xFF);
            LOGD("PMT PID: 0x%04X(%d)", program_map_PID, program_map_PID);
            pidsOfPMT.pids[indexOfPids++] = program_map_PID;
        }
        pidsOfPMT.number = indexOfPids;
        return sectionLength;
    }

    int parsePMT(uint8_t *pat){
        int staticLengthOfPMT = 12;
        int sectionLength = ((pat[1] & 0x0F) << 8) | (pat[2] & 0xFF);
        LOGD("Section Length: %d", sectionLength);
        int programInfoLength = ((pat[10] & 0x0F) << 8) | (pat[11] & 0xFF);
        LOGD("Program Info Length: %d", programInfoLength);

        int cursor = staticLengthOfPMT + programInfoLength;
        while (cursor < sectionLength - 1){
            int streamType = pat[cursor] & 0xFF;
            int elementaryPID = ((pat[cursor + 1] & 0x1F) << 8) | (pat[cursor + 2] & 0xFF);
            LOGD("Stream Type: 0x%02X(%d) Elementary PID: 0x%04X(%d)",
                streamType, streamType, elementaryPID, elementaryPID);

            //if (streamType == 0x04) LOGD("The type of this stream is MP3, which is not yet supported in this program.");
            //if (streamType == 0x0F || streamType == 0x11) relevantPID = elementaryPID;
            if (isStreamTypeActive((MTSStreamType)streamType)){
             relevantPID = elementaryPID; 
            }

            int esInfoLength = ((pat[cursor + 3] & 0x0F) << 8) | (pat[cursor + 4] & 0xFF);
            LOGD("ES Info Length: 0x%04X(%d)", esInfoLength, esInfoLength);
            cursor += 5 + esInfoLength;
        }
        return sectionLength;
    }

    int parsePES(uint8_t *pat, const int posOfPacketStart, const bool isNewPayload){
        int dataSize = 0; 
        if (isNewPayload){
            uint8_t streamID = pat[3] & 0xFF;
            LOGD("Stream ID:%02X ", streamID);
            const uint8_t posOfPacketLengthLatterHalf = 5;
            uint16_t pesRemainingPacketLength = ((pat[4] & 0xFF) << 8) | (pat[5] & 0xFF);
            LOGD("PES Packet length: %d", pesRemainingPacketLength);
            pesDataLength = pesRemainingPacketLength;
            const uint8_t posOfHeaderLength = 8;
            uint8_t pesRemainingHeaderLength = pat[posOfHeaderLength] & 0xFF;
            LOGD("PES Header length: %d", pesRemainingHeaderLength);
            int startOfData = posOfHeaderLength + pesRemainingHeaderLength + 1;
            dataSize = (TS_PACKET_SIZE - posOfPacketStart) - startOfData;
            if (p_print) p_print->write(&pat[startOfData], dataSize);
            pesDataLength -= (TS_PACKET_SIZE - posOfPacketStart) - (posOfPacketLengthLatterHalf + 1);
        } else {
            dataSize = TS_PACKET_SIZE - posOfPacketStart;
            if (p_print) p_print->write(pat, dataSize);
            pesDataLength -= dataSize;
        }
        return dataSize;
    }
};

}  // namespace audio_tools

