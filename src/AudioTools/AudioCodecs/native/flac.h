/*
 * flac_decoder.h
 *
 * Created on: Jul 03,2020
 * Updated on: Nov 23,2024
 *
 *      Author: wolle
 *
 *  Restrictions:
 *  blocksize must not exceed 16342 bytes
 *  bits per sample must be 8 or 16
 *  num Channels must be 1 or 2
 *
 *
 */
#pragma once
#pragma GCC optimize("Ofast")

#include <vector>

#include "Arduino.h"
#include "AudioLogger.h"
#include "native.h"

#define FLAC_MAX_CHANNELS 2
#define FLAC_MAX_BLOCKSIZE 16384
#define FLAC_MAX_OUTBUFFSIZE 4096 * 2

namespace audio_tools {

using namespace std;

enum : uint8_t { DECODER_INIT, DECODER_READ_IN, DECODER_WRITE_OUT };
enum : uint8_t { DECODE_FRAME, DECODE_SUBFRAMES, OUT_SAMPLES };
enum : int8_t {
  PARSE_OGG_DONE = 100,
  DECODE_FRAMES_LOOP = 100,
  OGG_SYNC_FOUND = +2,
  GIVE_NEXT_LOOP = +1,
  ERR_NONE = 0,
  ERR_BLOCKSIZE_TOO_BIG = -1,
  ERR_RESERVED_BLOCKSIZE_UNSUPPORTED = -2,
  ERR_SYNC_CODE_NOT_FOUND = -3,
  ERR_UNKNOWN_CHANNEL_ASSIGNMENT = -4,
  ERR_RESERVED_CHANNEL_ASSIGNMENT = -5,
  ERR_RESERVED_SUB_TYPE = -6,
  ERR_PREORDER_TOO_BIG = -7,
  ERR_RESERVED_RESIDUAL_CODING = -8,
  ERR_WRONG_RICE_PARTITION_NR = -9,
  ERR_BITS_PER_SAMPLE_TOO_BIG = -10,
  ERR_BITS_PER_SAMPLE_UNKNOWN = -11,
  ERR_DECODER_ASYNC = -12,
  ERR_UNIMPLEMENTED = -13,
  ERR_BITREADER_UNDERFLOW = -14,
  ERR_OUTBUFFER_TOO_SMALL = -15
};

struct MetadataBlock_t {
  // METADATA_BLOCK_STREAMINFO
  uint16_t
      minblocksize;  // The minimum block size (in samples) used in the stream.
                     //
                     // The maximum block size (in samples) used in the stream.
  uint16_t
      maxblocksize;  // (Minimum blocksize == maximum blocksize) implies a
                     // fixed-blocksize stream.
                     //
                     // The minimum frame size (in bytes) used in the stream.
  uint32_t
      minframesize;  // May be 0 to imply the value is not known.
                     //
                     // The maximum frame size (in bytes) used in the stream.
  uint32_t maxframesize;  // May be 0 to imply the value is not known.
                          //
                          // Sample rate in Hz. Though 20 bits are available,
                          // the maximum sample rate is limited by the structure
                          // of frame headers to 655350Hz.
  uint32_t sampleRate;  // Also, a value of 0 is invalid.
                        //
                        // Number of channels  supports from 1 to 8 channels
  uint8_t numChannels;  // 000 : 1 channel .... 111 : 8 channels
                        //
                        // Sample size in bits:
                        // 000 : get from STREAMINFO metadata block
                        // 001 : 8 bits per sample
                        // 010 : 12 bits per sample
                        // 011 : reserved
                        // 100 : 16 bits per sample
                        // 101 : 20 bits per sample
                        // 110 : 24 bits per sample
  uint8_t
      bitsPerSample;  // 111 : reserved
                      //
                      // Total samples in stream. 'Samples' means inter-channel
                      // sample, i.e. one second of 44.1Khz audio will have
                      // 44100 samples regardless of the number
  uint64_t totalSamples;  // of channels. A value of zero here means the number
                          // of total samples is unknown.
                          //
  uint32_t audioDataLength;  // is not the filelength, is only the length of the
                             // audio datablock in bytes
};

struct FrameHeader_t {
  // 0 : fixed-blocksize stream; frame header encodes the frame number
  uint8_t blockingStrategy;  // 1 : variable-blocksize stream; frame header
                             // encodes the sample number
                             //
                             // Block size in inter-channel samples:
                             // 0000 : reserved
                             // 0001 : 192 samples
                             // 0010-0101 : 576 * (2^(n-2)) samples, i.e.
                             // 576/1152/2304/4608 0110 : get 8 bit
                             // (blocksize-1) from end of header 0111 : get 16
                             // bit (blocksize-1) from end of header
  uint8_t blockSizeCode;  // 1000-1111 : 256 * (2^(n-8)) samples, i.e.
                          // 256/512/1024/2048/4096/8192/16384/32768
                          //
                          // 0000 : get from STREAMINFO metadata block
                          // 0001 : 88.2kHz
                          // 0010 : 176.4kHz
                          // 0011 : 192kHz
                          // 0100 : 8kHz
                          // 0101 : 16kHz
                          // 0110 : 22.05kHz
                          // 0111 : 24kHz
                          // 1000 : 32kHz
                          // 1001 : 44.1kHz
                          // 1010 : 48kHz
                          // 1011 : 96kHz
                          // 1100 : get 8 bit sample rate (in kHz) from end of
                          // header 1101 : get 16 bit sample rate (in Hz) from
                          // end of header 1110 : get 16 bit sample rate (in
                          // tens of Hz) from end of header
  uint8_t
      sampleRateCode;  // 1111 : invalid, to prevent sync-fooling string of 1s
                       //
                       // Channel assignment
                       // 0000 1 channel: mono
                       // 0001 2 channels: left, right
                       // 0010 3 channels
                       // 0011 4 channels
                       // 0100 5 channels
                       // 0101 6 channels
                       // 0110 7 channels
                       // 0111 8 channels
                       // 1000 : left/side stereo: channel 0 is the left
                       // channel, channel 1 is the side(difference) channel
                       // 1001 : right/side stereo: channel 0 is the
                       // side(difference) channel, channel 1 is the right
                       // channel 1010 : mid/side stereo: channel 0 is the
                       // mid(average) channel, channel 1 is the
                       // side(difference) channel
  uint8_t chanAsgn;    // 1011-1111 : reserved
                       //
                       // Sample size in bits:
                       // 000 : get from STREAMINFO metadata block
                       // 001 : 8 bits per sample
                       // 010 : 12 bits per sample
                       // 011 : reserved
                       // 100 : 16 bits per sample
                       // 101 : 20 bits per sample
                       // 110 : 24 bits per sample
  uint8_t sampleSizeCode;  // 111 : reserved
                           //
  uint32_t totalSamples;   // totalSamplesInStream
                           //
  uint32_t bitrate;        // bitrate
};

class DecoderFLACNative : public DecoderNative {

 protected:
  FrameHeader_t* s_frameHeader;
  MetadataBlock_t* s_metadataBlock;
  vector<uint32_t> s_flacSegmTableVec;
  vector<int32_t> coefs;
  vector<uint32_t> s_flacBlockPicItem;
  uint64_t s_flac_bitBuffer = 0;
  uint32_t s_flacBitrate = 0;
  uint32_t s_flacBlockPicLenUntilFrameEnd = 0;
  uint32_t s_flacCurrentFilePos = 0;
  uint32_t s_flacBlockPicPos = 0;
  uint32_t s_flacBlockPicLen = 0;
  uint32_t s_flacAudioDataStart = 0;
  int32_t s_flacRemainBlockPicLen = 0;
  const uint16_t s_flacOutBuffSize = 2048;
  uint16_t s_numOfOutSamples = 0;
  uint16_t s_flacValidSamples = 0;
  uint16_t s_rIndex = 0;
  uint16_t s_offset = 0;
  uint8_t s_flacStatus = 0;
  uint8_t* s_flacInptr;
  float s_flacCompressionRatio = 0;
  uint8_t s_flacBitBufferLen = 0;
  bool s_f_flacParseOgg = false;
  bool s_f_bitReaderError = false;
  uint8_t s_flac_pageSegments = 0;
  char* s_flacStreamTitle = NULL;
  char* s_flacVendorString = NULL;
  bool s_f_flacNewStreamtitle = false;
  bool s_f_flacFirstCall = true;
  bool s_f_oggWrapper = false;
  bool s_f_lastMetaDataBlock = false;
  bool s_f_flacNewMetadataBlockPicture = false;
  uint8_t s_flacPageNr = 0;
  int32_t** s_samplesBuffer = NULL;
  uint16_t s_maxBlocksize = FLAC_MAX_BLOCKSIZE;
  int32_t s_nBytes = 0;

  //           INI SECTION

  bool allocateBuffers(void) {
    if (!s_frameHeader) {
      s_frameHeader = (FrameHeader_t*)codec_malloc(sizeof(FrameHeader_t));
    }
    if (!s_metadataBlock) {
      s_metadataBlock =
          (MetadataBlock_t*)codec_malloc(sizeof(MetadataBlock_t));
    }
    if (!s_flacStreamTitle) {
      s_flacStreamTitle = (char*)codec_malloc(256);
    }

    if (!s_frameHeader || !s_metadataBlock || !s_flacStreamTitle) {
      LOGE("not enough memory to allocate flacdecoder buffers");
      return false;
    }

    s_samplesBuffer =
        (int32_t**)codec_malloc(FLAC_MAX_CHANNELS * sizeof(int32_t*));
    for (int32_t i = 0; i < FLAC_MAX_CHANNELS; i++) {
      s_samplesBuffer[i] =
          (int32_t*)codec_malloc(s_maxBlocksize * sizeof(int32_t));
      if (!s_samplesBuffer[i]) {
        LOGE("not enough memory to allocate flacdecoder buffers");
        return false;
      }
    }

    decoder_ClearBuffer();
    decoder_setDefaults();
    s_flacPageNr = 0;
    return true;
  }

  void decoder_ClearBuffer() {
    memset(s_frameHeader, 0, sizeof(FrameHeader_t));
    memset(s_metadataBlock, 0, sizeof(MetadataBlock_t));

    if (s_samplesBuffer) {
      for (int32_t i = 0; i < FLAC_MAX_CHANNELS; i++) {
        memset(s_samplesBuffer[i], 0, s_maxBlocksize * sizeof(int32_t));
      }
    }

    s_flacSegmTableVec.clear();
    s_flacSegmTableVec.shrink_to_fit();
    s_flacStatus = DECODE_FRAME;
    return;
  }

  void freeBuffers() {
    if (s_frameHeader) {
      free(s_frameHeader);
      s_frameHeader = NULL;
    }
    if (s_metadataBlock) {
      free(s_metadataBlock);
      s_metadataBlock = NULL;
    }
    if (s_flacStreamTitle) {
      free(s_flacStreamTitle);
      s_flacStreamTitle = NULL;
    }
    if (s_flacVendorString) {
      free(s_flacVendorString);
      s_flacVendorString = NULL;
    }

    if (s_samplesBuffer) {
      for (int32_t i = 0; i < FLAC_MAX_CHANNELS; i++) {
        if (s_samplesBuffer[i]) {
          free(s_samplesBuffer[i]);
        }
      }
      free(s_samplesBuffer);
      s_samplesBuffer = NULL;
    }
    coefs.clear();
    coefs.shrink_to_fit();
    s_flacSegmTableVec.clear();
    s_flacSegmTableVec.shrink_to_fit();
    s_flacBlockPicItem.clear();
    s_flacBlockPicItem.shrink_to_fit();
  }

  void decoder_setDefaults() {
    coefs.clear();
    coefs.shrink_to_fit();
    s_flacSegmTableVec.clear();
    s_flacSegmTableVec.shrink_to_fit();
    s_flacBlockPicItem.clear();
    s_flacBlockPicItem.shrink_to_fit();
    s_flac_bitBuffer = 0;
    s_flacBitrate = 0;
    s_flacBlockPicLenUntilFrameEnd = 0;
    s_flacCurrentFilePos = 0;
    s_flacBlockPicPos = 0;
    s_flacBlockPicLen = 0;
    s_flacRemainBlockPicLen = 0;
    s_flacAudioDataStart = 0;
    s_numOfOutSamples = 0;
    s_offset = 0;
    s_flacValidSamples = 0;
    s_rIndex = 0;
    s_flacStatus = DECODE_FRAME;
    s_flacCompressionRatio = 0;
    s_flacBitBufferLen = 0;
    s_flac_pageSegments = 0;
    s_f_flacNewStreamtitle = false;
    s_f_flacFirstCall = true;
    s_f_oggWrapper = false;
    s_f_lastMetaDataBlock = false;
    s_f_flacNewMetadataBlockPicture = false;
    s_f_flacParseOgg = false;
    s_f_bitReaderError = false;
    s_nBytes = 0;
  }

  //            B I T R E A D E R

  const uint32_t mask[] = {
      0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000f, 0x0000001f,
      0x0000003f, 0x0000007f, 0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
      0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff, 0x0001ffff,
      0x0003ffff, 0x0007ffff, 0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
      0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff, 0x1fffffff,
      0x3fffffff, 0x7fffffff, 0xffffffff};

  uint32_t readUint(uint8_t nBits, int32_t* bytesLeft) {
    while (s_flacBitBufferLen < nBits) {
      uint8_t temp = *(s_flacInptr + s_rIndex);
      s_rIndex++;
      (*bytesLeft);
      if (*bytesLeft < 0) {
        LOGE("error in bitreader");
        s_f_bitReaderError = true;
        break;
      }
      s_flac_bitBuffer = (s_flac_bitBuffer << 8) | temp;
      s_flacBitBufferLen += 8;
    }
    s_flacBitBufferLen -= nBits;
    uint32_t result = s_flac_bitBuffer >> s_flacBitBufferLen;
    if (nBits < 32) result &= mask[nBits];
    return result;
  }

  int32_t readSignedInt(int32_t nBits, int32_t* bytesLeft) {
    int32_t temp = readUint(nBits, bytesLeft) << (32 - nBits);
    temp = temp >> (32 - nBits);  // The C++ compiler uses the sign bit to fill
                                  // vacated bit positions
    return temp;
  }

  int64_t readRiceSignedInt(uint8_t param, int32_t* bytesLeft) {
    long val = 0;
    while (readUint(1, bytesLeft) == 0) val++;
    val = (val << param) | readUint(param, bytesLeft);
    return (val >> 1) ^ -(val & 1);
  }

  void alignToByte() { s_flacBitBufferLen -= s_flacBitBufferLen % 8; }

  //              F L A C - D E C O D E R

  void SetRawBlockParams(uint8_t Chans, uint32_t SampRate, uint8_t BPS,
                         uint32_t tsis, uint32_t AuDaLength) {
    s_metadataBlock->numChannels = Chans;
    s_metadataBlock->sampleRate = SampRate;
    s_metadataBlock->bitsPerSample = BPS;
    s_metadataBlock->totalSamples = tsis;  // total samples in stream
    s_metadataBlock->audioDataLength = AuDaLength;
  }

  void DecoderReset() {  // set var to default
    decoder_setDefaults();
    decoder_ClearBuffer();
  }

  int32_t findSyncWord(unsigned char* buf, int32_t nBytes) {
    int32_t i = specialIndexOf(buf, "OggS", nBytes);
    if (i == 0) {
      s_f_bitReaderError = false;
      return 0;
    }  // flag has ogg wrapper

    if (s_f_oggWrapper && i > 0) {
      s_f_bitReaderError = false;
      return i;
    } else {
      /* find byte-aligned sync code - need 14 matching bits */
      for (i = 0; i < nBytes - 1; i++) {
        if ((buf[i + 0] & 0xFF) == 0xFF &&
            (buf[i + 1] & 0xFC) == 0xF8) {  // <14> Sync code '11111111111110xx'
          if (i) DecoderReset();
          //    s_f_bitReaderError = false;
          return i;
        }
      }
    }
    return -1;
  }

  boolean findMagicWord(unsigned char* buf, int32_t nBytes) {
    int32_t idx = specialIndexOf(buf, "fLaC", nBytes);
    if (idx > 0) {  // Metadatablock follows
      idx += 4;
      boolean lmdbf =
          ((buf[idx + 1] & 0x80) == 0x80);  // Last-metadata-block flag
      uint8_t bt = (buf[idx + 1] & 0x7F);   // block type
      uint32_t lomd = (buf[idx + 2] << 16) + (buf[idx + 3] << 8) +
                      buf[idx + 4];  // Length of metadata to follow

      (void)lmdbf;
      (void)bt;
      (void)lomd;
      // log_i("Last-metadata-block flag: %d", lmdbf);
      // log_i("block type: %d", bt);
      // log_i("Length (in bytes) of metadata to follow: %d", lomd);
      return true;
    }
    return false;
  }

  char* getStreamTitle() {
    if (s_f_flacNewStreamtitle) {
      s_f_flacNewStreamtitle = false;
      return s_flacStreamTitle;
    }
    return NULL;
  }

  int32_t parseOGG(
      uint8_t* inbuf,
      int32_t*
          bytesLeft) {  // reference https://www.xiph.org/ogg/doc/rfc3533.txt

    s_f_flacParseOgg = false;
    int32_t idx = specialIndexOf(inbuf, "OggS", 6);
    if (idx != 0) return ERR_DECODER_ASYNC;

    uint8_t version = *(inbuf + 4);
    (void)version;
    uint8_t headerType = *(inbuf + 5);
    (void)headerType;
    uint64_t granulePosition =
        (uint64_t)*(inbuf + 13)
        << 56;  // granule_position: an 8 Byte field containing -
    granulePosition +=
        (uint64_t)*(inbuf + 12)
        << 48;  // position information. For an audio stream, it MAY
    granulePosition +=
        (uint64_t)*(inbuf + 11)
        << 40;  // contain the total number of PCM samples encoded
    granulePosition +=
        (uint64_t)*(inbuf + 10)
        << 32;  // after including all frames finished on this page.
    granulePosition +=
        *(inbuf + 9)
        << 24;  // This is a hint for the decoder and gives it some timing
    granulePosition +=
        *(inbuf + 8)
        << 16;  // and position information. A special value of -1 (in two's
    granulePosition +=
        *(inbuf + 7)
        << 8;  // complement) indicates that no packets finish on this page.
    granulePosition += *(inbuf + 6);
    (void)granulePosition;
    uint32_t bitstreamSerialNr =
        *(inbuf + 17)
        << 24;  // bitstream_serial_number: a 4 Byte field containing the
    bitstreamSerialNr +=
        *(inbuf + 16)
        << 16;  // unique serial number by which the logical bitstream
    bitstreamSerialNr += *(inbuf + 15) << 8;  // is identified.
    bitstreamSerialNr += *(inbuf + 14);
    (void)bitstreamSerialNr;
    uint32_t pageSequenceNr =
        *(inbuf + 21)
        << 24;  // page_sequence_number: a 4 Byte field containing the sequence
    pageSequenceNr +=
        *(inbuf + 20)
        << 16;  // number of the page so the decoder can identify page loss
    pageSequenceNr +=
        *(inbuf + 19)
        << 8;  // This sequence number is increasing on each logical bitstream
    pageSequenceNr += *(inbuf + 18);
    (void)pageSequenceNr;
    uint32_t CRCchecksum = *(inbuf + 25) << 24;
    CRCchecksum += *(inbuf + 24) << 16;
    CRCchecksum += *(inbuf + 23) << 8;
    CRCchecksum += *(inbuf + 22);
    (void)CRCchecksum;
    uint8_t pageSegments =
        *(inbuf + 26);  // giving the number of segment entries

    // read the segment table (contains pageSegments bytes),  1...251: Length of
    // the frame in bytes, 255: A second byte is needed.  The total length is
    // first_byte + second byte
    s_flacSegmTableVec.clear();
    s_flacSegmTableVec.shrink_to_fit();
    for (int32_t i = 0; i < pageSegments; i++) {
      int32_t n = *(inbuf + 27 + i);
      while (*(inbuf + 27 + i) == 255) {
        i++;
        if (i == pageSegments) break;
        n += *(inbuf + 27 + i);
      }
      s_flacSegmTableVec.insert(s_flacSegmTableVec.begin(), n);
    }
    // for(int32_t i = 0; i< s_flacSegmTableVec.size(); i++){log_w("%i",
    // s_flacSegmTableVec[i]);}

    bool continuedPage =
        headerType & 0x01;  // set: page contains data of a packet continued
                            // from the previous page
    bool firstPage =
        headerType &
        0x02;  // set: this is the first page of a logical bitstream (bos)
    bool lastPage =
        headerType &
        0x04;  // set: this is the last page of a logical bitstream (eos)

    (void)continuedPage;
    (void)lastPage;

    // log_w("firstPage %i, continuedPage %i, lastPage %i", firstPage,
    // continuedPage, lastPage);

    if (firstPage) s_flacPageNr = 0;

    uint32_t headerSize = pageSegments + 27;

    *bytesLeft -= headerSize;
    s_flacCurrentFilePos += headerSize;
    return ERR_NONE;  // no error
  }

  vector<uint32_t> getMetadataBlockPicture() {
    if (s_f_flacNewMetadataBlockPicture) {
      s_f_flacNewMetadataBlockPicture = false;
      return s_flacBlockPicItem;
    }
    if (s_flacBlockPicItem.size() > 0) {
      s_flacBlockPicItem.clear();
      s_flacBlockPicItem.shrink_to_fit();
    }
    return s_flacBlockPicItem;
  }
  int32_t parseFlacFirstPacket(
      uint8_t* inbuf,
      int16_t nBytes) {  // 4.2.2. Identification header
                         // https://xiph.org/flac/ogg_mapping.html

    int32_t ret = 0;
    int32_t idx = specialIndexOf(inbuf, "fLaC", nBytes);
    // log_i("idx %i, nBytes %i", idx, nBytes);
    if (idx >= 0) {  //  signature found
      ret = idx + 4;
    } else {
      LOGE(" signature not found");
      ret = ERR_DECODER_ASYNC;
    }
    return ret;
  }
  int32_t parseMetaDataBlockHeader(uint8_t* inbuf, int16_t nBytes) {
    int8_t ret = PARSE_OGG_DONE;
    uint16_t pos = 0;
    int32_t blockLength = 0;
    uint16_t minBlocksize = 0;
    uint16_t maxBlocksize = 0;
    uint32_t minFrameSize = 0;
    uint32_t maxFrameSize = 0;
    uint32_t sampleRate = 0;
    uint32_t vendorLength = 0;
    uint32_t commemtStringLength = 0;
    uint32_t userCommentListLength = 0;
    uint8_t nrOfChannels = 0;
    uint8_t bitsPerSample = 0;
    uint64_t totalSamplesInStream = 0;
    uint8_t mdBlockHeader = 0;
    uint8_t blockType = 0;
    uint8_t bt = 0;
    char* vb[8] = {0};  // vorbis comment

    enum {
      streamInfo,
      padding,
      application,
      seekTable,
      vorbisComment,
      cueSheet,
      picture
    };

    while (true) {
      mdBlockHeader = *(inbuf + pos);
      s_f_lastMetaDataBlock =
          mdBlockHeader &
          0b10000000;  // log_w("lastMdBlockFlag %i", s_f_lastMetaDataBlock);
      blockType =
          mdBlockHeader & 0b01111111;  // log_w("blockType %i", blockType);

      blockLength = *(inbuf + pos + 1) << 16;
      blockLength += *(inbuf + pos + 2) << 8;
      blockLength +=
          *(inbuf + pos + 3);  // log_w("blockLength %i", blockLength);

      nBytes -= 4;
      pos += 4;

      switch (blockType) {
        case 0:
          bt = streamInfo;
          break;
        case 1:
          bt = padding;
          //  LOGE("padding");
          return ERR_NONE;
          break;
        case 2:
          bt = application;
          LOGE("application");
          return ERR_UNIMPLEMENTED;
          break;
        case 3:
          bt = seekTable;
          LOGE("seekTable");
          return ERR_UNIMPLEMENTED;
          break;
        case 4:
          bt = vorbisComment;
          break;
        case 5:
          bt = cueSheet;
          LOGE("cueSheet");
          return ERR_UNIMPLEMENTED;
          break;
        case 6:
          bt = picture;
          break;
        default:
          bt = streamInfo;
          // return ERR_UNIMPLEMENTED;
          break;
      }

      switch (bt) {
        case streamInfo:
          minBlocksize += *(inbuf + pos + 0) << 8;
          minBlocksize += *(inbuf + pos + 1);
          maxBlocksize += *(inbuf + pos + 2) << 8;
          maxBlocksize += *(inbuf + pos + 3);
          // log_i("minBlocksize %i", minBlocksize);
          // log_i("maxBlocksize %i", maxBlocksize);
          s_metadataBlock->minblocksize = minBlocksize;
          s_metadataBlock->maxblocksize = maxBlocksize;

          if (maxBlocksize > s_maxBlocksize) {
            LOGE("s_blocksize is too big");
            return ERR_BLOCKSIZE_TOO_BIG;
          }

          minFrameSize = *(inbuf + pos + 4) << 16;
          minFrameSize += *(inbuf + pos + 5) << 8;
          minFrameSize += *(inbuf + pos + 6);
          maxFrameSize = *(inbuf + pos + 7) << 16;
          maxFrameSize += *(inbuf + pos + 8) << 8;
          maxFrameSize += *(inbuf + pos + 9);
          // log_i("minFrameSize %i", minFrameSize);
          // log_i("maxFrameSize %i", maxFrameSize);
          s_metadataBlock->minframesize = minFrameSize;
          s_metadataBlock->maxframesize = maxFrameSize;

          sampleRate = *(inbuf + pos + 10) << 12;
          sampleRate += *(inbuf + pos + 11) << 4;
          sampleRate += (*(inbuf + pos + 12) & 0xF0) >> 4;
          // log_i("sampleRate %i", sampleRate);
          s_metadataBlock->sampleRate = sampleRate;

          nrOfChannels = ((*(inbuf + pos + 12) & 0x0E) >> 1) + 1;
          // log_i("nrOfChannels %i", nrOfChannels);
          s_metadataBlock->numChannels = nrOfChannels;

          bitsPerSample = (*(inbuf + pos + 12) & 0x01) << 5;
          bitsPerSample += ((*(inbuf + pos + 13) & 0xF0) >> 4) + 1;
          s_metadataBlock->bitsPerSample = bitsPerSample;
          // log_i("bitsPerSample %i", bitsPerSample);

          totalSamplesInStream = (uint64_t)(*(inbuf + pos + 17) & 0x0F) << 32;
          totalSamplesInStream += (*(inbuf + pos + 14)) << 24;
          totalSamplesInStream += (*(inbuf + pos + 14)) << 16;
          totalSamplesInStream += (*(inbuf + pos + 15)) << 8;
          totalSamplesInStream += (*(inbuf + pos + 16));
          // log_i("totalSamplesInStream %lli", totalSamplesInStream);
          s_metadataBlock->totalSamples = totalSamplesInStream;

          // log_i("nBytes %i, blockLength %i", nBytes, blockLength);
          pos += blockLength;
          nBytes -= blockLength;
          if (ret == PARSE_OGG_DONE) return ret;
          break;

        case vorbisComment:  // https://www.xiph.org/vorbis/doc/v-comment.html
          vendorLength = *(inbuf + pos + 3) << 24;
          vendorLength += *(inbuf + pos + 2) << 16;
          vendorLength += *(inbuf + pos + 1) << 8;
          vendorLength += *(inbuf + pos + 0);
          if (vendorLength > 1024) {
            LOGE("vendorLength > 1024 bytes");
          }
          if (s_flacVendorString) {
            free(s_flacVendorString);
            s_flacVendorString = NULL;
          }
          s_flacVendorString =
              (char*)codec_calloc(vendorLength + 1, sizeof(char));
          memcpy(s_flacVendorString, inbuf + pos + 4, vendorLength);
          // log_i("%s", s_flacVendorString);

          pos += 4 + vendorLength;
          userCommentListLength = *(inbuf + pos + 3) << 24;
          userCommentListLength += *(inbuf + pos + 2) << 16;
          userCommentListLength += *(inbuf + pos + 1) << 8;
          userCommentListLength += *(inbuf + pos + 0);

          pos += 4;
          commemtStringLength = 0;
          for (int32_t i = 0; i < userCommentListLength; i++) {
            commemtStringLength = *(inbuf + pos + 3) << 24;
            commemtStringLength += *(inbuf + pos + 2) << 16;
            commemtStringLength += *(inbuf + pos + 1) << 8;
            commemtStringLength += *(inbuf + pos + 0);

            if ((specialIndexOf(inbuf + pos + 4, "TITLE", 6) == 0) ||
                (specialIndexOf(inbuf + pos + 4, "title", 6) == 0)) {
              vb[0] = flac_x_ps_strndup(
                  (const char*)(inbuf + pos + 4 + 6),
                  min((uint32_t)127, commemtStringLength - 6));
              // log_w("TITLE: %s", vb[0]);
            }
            if ((specialIndexOf(inbuf + pos + 4, "ARTIST", 7) == 0) ||
                (specialIndexOf(inbuf + pos + 4, "artist", 7) == 0)) {
              vb[1] = flac_x_ps_strndup(
                  (const char*)(inbuf + pos + 4 + 7),
                  min((uint32_t)127, commemtStringLength - 7));
              // log_w("ARTIST: %s", vb[1]);
            }
            if ((specialIndexOf(inbuf + pos + 4, "GENRE", 6) == 0) ||
                (specialIndexOf(inbuf + pos + 4, "genre", 6) == 0)) {
              vb[2] = flac_x_ps_strndup(
                  (const char*)(inbuf + pos + 4 + 6),
                  min((uint32_t)127, commemtStringLength - 6));
              // log_w("GENRE: %s", vb[2]);
            }
            if ((specialIndexOf(inbuf + pos + 4, "ALBUM", 6) == 0) ||
                (specialIndexOf(inbuf + pos + 4, "album", 6) == 0)) {
              vb[3] = flac_x_ps_strndup(
                  (const char*)(inbuf + pos + 4 + 6),
                  min((uint32_t)127, commemtStringLength - 6));
              // log_w("ALBUM: %s", vb[3]);
            }
            if ((specialIndexOf(inbuf + pos + 4, "COMMENT", 8) == 0) ||
                (specialIndexOf(inbuf + pos + 4, "comment", 8) == 0)) {
              vb[4] = flac_x_ps_strndup(
                  (const char*)(inbuf + pos + 4 + 8),
                  min((uint32_t)127, commemtStringLength - 8));
              // log_w("COMMENT: %s", vb[4]);
            }
            if ((specialIndexOf(inbuf + pos + 4, "DATE", 5) == 0) ||
                (specialIndexOf(inbuf + pos + 4, "date", 5) == 0)) {
              vb[5] = flac_x_ps_strndup(
                  (const char*)(inbuf + pos + 4 + 5),
                  min((uint32_t)127, commemtStringLength - 12));
              // log_w("DATE: %s", vb[5]);
            }
            if ((specialIndexOf(inbuf + pos + 4, "TRACKNUMBER", 12) == 0) ||
                (specialIndexOf(inbuf + pos + 4, "tracknumber", 12) == 0)) {
              vb[6] = flac_x_ps_strndup(
                  (const char*)(inbuf + pos + 4 + 12),
                  min((uint32_t)127, commemtStringLength - 12));
              // log_w("TRACKNUMBER: %s", vb[6]);
            }
            if ((specialIndexOf(inbuf + pos + 4, "METADATA_BLOCK_PICTURE",
                                23) == 0) ||
                (specialIndexOf(inbuf + pos + 4, "metadata_block_picture",
                                23) == 0)) {
              // log_w("METADATA_BLOCK_PICTURE found, commemtStringLength %i",
              // commemtStringLength);
              s_flacBlockPicLen = commemtStringLength - 23;
              s_flacBlockPicPos = s_flacCurrentFilePos + pos + 4 + 23;
              s_flacBlockPicLenUntilFrameEnd = nBytes - (pos + 23);
              if (s_flacBlockPicLen < s_flacBlockPicLenUntilFrameEnd)
                s_flacBlockPicLenUntilFrameEnd = s_flacBlockPicLen;
              s_flacRemainBlockPicLen =
                  s_flacBlockPicLen - s_flacBlockPicLenUntilFrameEnd;
              // log_i("s_flacBlockPicPos %i, s_flacBlockPicLen %i",
              // s_flacBlockPicPos, s_flacBlockPicLen);
              // log_i("s_flacBlockPicLenUntilFrameEnd %i,
              // s_flacRemainBlockPicLen %i", s_flacBlockPicLenUntilFrameEnd,
              // s_flacRemainBlockPicLen);
              if (s_flacRemainBlockPicLen <= 0)
                s_f_lastMetaDataBlock =
                    true;  // exeption:: goto audiopage after commemt if
                           // lastMetaDataFlag is not set
              if (s_flacBlockPicLen) {
                s_flacBlockPicItem.clear();
                s_flacBlockPicItem.shrink_to_fit();
                s_flacBlockPicItem.push_back(s_flacBlockPicPos);
                s_flacBlockPicItem.push_back(s_flacBlockPicLenUntilFrameEnd);
              }
            }
            pos += 4 + commemtStringLength;
            // log_i("nBytes %i, pos %i, commemtStringLength %i", nBytes, pos,
            // commemtStringLength);
          }
          memset(s_flacStreamTitle, 0, 256);
          if (vb[1] && vb[0]) {  // artist and title
            strcpy(s_flacStreamTitle, vb[1]);
            strcat(s_flacStreamTitle, " - ");
            strcat(s_flacStreamTitle, vb[0]);
            s_f_flacNewStreamtitle = true;
          } else if (vb[1]) {
            strcpy(s_flacStreamTitle, vb[1]);
            s_f_flacNewStreamtitle = true;
          } else if (vb[0]) {
            strcpy(s_flacStreamTitle, vb[0]);
            s_f_flacNewStreamtitle = true;
          }
          for (int32_t i = 0; i < 8; i++) {
            if (vb[i]) {
              free(vb[i]);
              vb[i] = NULL;
            }
          }

          if (!s_flacBlockPicLen && s_flacSegmTableVec.size() == 1)
            s_f_lastMetaDataBlock =
                true;  // exeption:: goto audiopage after commemt if
                       // lastMetaDataFlag is not set
          if (ret == PARSE_OGG_DONE) return ret;
          break;

        case picture:
          if (ret == PARSE_OGG_DONE) return ret;
          break;

        default:
          return ret;
          break;
      }
    }
    return 0;
  }

  int8_t Decode(uint8_t* inbuf, int32_t* bytesLeft,
                int16_t* outbuf) {  //  MAIN LOOP

    int32_t ret = 0;
    uint32_t segmLen = 0;
    static uint32_t segmLenTmp = 0;

    if (s_f_flacFirstCall) {  // determine if ogg or flag
      s_f_flacFirstCall = false;
      s_nBytes = 0;
      segmLenTmp = 0;
      if (specialIndexOf(inbuf, "OggS", 5) == 0) {
        s_f_oggWrapper = true;
        s_f_flacParseOgg = true;
      }
    }

    if (s_f_oggWrapper) {
      if (segmLenTmp) {  // can't skip more than 16K
        if (segmLenTmp > 16384) {
          s_flacCurrentFilePos += 16384;
          *bytesLeft -= 16384;
          segmLenTmp -= 16384;
        } else {
          s_flacCurrentFilePos += segmLenTmp;
          *bytesLeft -= segmLenTmp;
          segmLenTmp = 0;
        }
        return PARSE_OGG_DONE;
      }

      if (s_nBytes > 0) {
        int16_t diff = s_nBytes;
        if (s_flacAudioDataStart == 0) {
          s_flacAudioDataStart = s_flacCurrentFilePos;
        }
        ret = DecodeNative(inbuf, &s_nBytes, outbuf);
        diff -= s_nBytes;
        s_flacCurrentFilePos += diff;
        *bytesLeft -= diff;
        return ret;
      }
      if (s_nBytes < 0) {
        return ERR_DECODER_ASYNC;
      }

      if (s_f_flacParseOgg == true) {
        s_f_flacParseOgg = false;
        ret = parseOGG(inbuf, bytesLeft);
        if (ret == ERR_NONE)
          return PARSE_OGG_DONE;  // ok
        else
          return ret;  // error
      }
      //-
      if (!s_flacSegmTableVec.size()) LOGE("size is 0");
      segmLen = s_flacSegmTableVec.back();
      s_flacSegmTableVec.pop_back();
      if (!s_flacSegmTableVec.size()) s_f_flacParseOgg = true;
      //-

      if (s_flacRemainBlockPicLen <= 0 && !s_f_flacNewMetadataBlockPicture) {
        if (s_flacBlockPicItem.size() > 0) {  // get blockpic data
          // log_i("-");
          // log_i("metadata blockpic found at pos %i, size %i bytes",
          // s_flacBlockPicPos, s_flacBlockPicLen); for(int32_t i = 0; i <
          // s_flacBlockPicItem.size(); i += 2) { log_i("segment %02i, pos %07i,
          // len %05i", i / 2, s_flacBlockPicItem[i], s_flacBlockPicItem[i +
          // 1]); }
          // log_i("-");
          s_f_flacNewMetadataBlockPicture = true;
        }
      }

      switch (s_flacPageNr) {
        case 0:
          ret = parseFlacFirstPacket(inbuf, segmLen);
          if (ret == segmLen) {
            s_flacPageNr = 1;
            ret = PARSE_OGG_DONE;
            break;
          }
          if (ret < 0) {  // fLaC signature not found
            break;
          }
          if (ret < segmLen) {
            segmLen -= ret;
            *bytesLeft -= ret;
            s_flacCurrentFilePos += ret;
            inbuf += ret;
            s_flacPageNr = 1;
          } /* fallthrough */
        case 1:
          if (s_flacRemainBlockPicLen > 0) {
            s_flacRemainBlockPicLen -= segmLen;
            // log_i("s_flacCurrentFilePos %i, len %i, s_flacRemainBlockPicLen
            // %i", s_flacCurrentFilePos, segmLen, s_flacRemainBlockPicLen);
            s_flacBlockPicItem.push_back(s_flacCurrentFilePos);
            s_flacBlockPicItem.push_back(segmLen);
            if (s_flacRemainBlockPicLen <= 0) {
              s_flacPageNr = 2;
            }
            ret = PARSE_OGG_DONE;
            break;
          }
          ret = parseMetaDataBlockHeader(inbuf, segmLen);
          if (s_f_lastMetaDataBlock) s_flacPageNr = 2;
          break;
        case 2:
          s_nBytes = segmLen;
          return PARSE_OGG_DONE;
          break;
      }
      if (segmLen > 16384) {
        segmLenTmp = segmLen;
        return PARSE_OGG_DONE;
      }
      *bytesLeft -= segmLen;
      s_flacCurrentFilePos += segmLen;
      return ret;
    }
    ret = DecodeNative(inbuf, bytesLeft, outbuf);
    return ret;
  }

  int8_t DecodeNative(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf) {
    int32_t bl = *bytesLeft;
    static int32_t sbl = 0;

    if (s_flacStatus != OUT_SAMPLES) {
      s_rIndex = 0;
      s_flacInptr = inbuf;
    }

    while (
        s_flacStatus ==
        DECODE_FRAME) {  // Read a ton of header fields, and ignore most of them
      int32_t ret = flacDecodeFrame(inbuf, bytesLeft);
      if (ret != 0) return ret;
      if (*bytesLeft < FLAC_MAX_BLOCKSIZE)
        return DECODE_FRAMES_LOOP;  // need more data
      sbl += bl - *bytesLeft;
    }

    if (s_flacStatus == DECODE_SUBFRAMES) {
      // Decode each channel's subframe, then skip footer
      int32_t ret = decodeSubframes(bytesLeft);
      if (ret != 0) return ret;
      s_flacStatus = OUT_SAMPLES;
      sbl += bl - *bytesLeft;
    }

    if (s_flacStatus == OUT_SAMPLES) {  // Write the decoded samples
      // blocksize can be much greater than outbuff, so we can't stuff all in
      // once therefore we need often more than one loop (split outputblock into
      // pieces)
      uint16_t blockSize;
      if (s_numOfOutSamples < s_flacOutBuffSize + s_offset)
        blockSize = s_numOfOutSamples - s_offset;
      else
        blockSize = s_flacOutBuffSize;

      for (int32_t i = 0; i < blockSize; i++) {
        for (int32_t j = 0; j < s_metadataBlock->numChannels; j++) {
          int32_t val = s_samplesBuffer[j][i + s_offset];
          if (MetadataBlock->bitsPerSample == 8) val += 128;
          outbuf[2 * i + j] = val;
        }
      }

      s_flacValidSamples = blockSize * s_metadataBlock->numChannels;
      s_offset += blockSize;
      if (sbl > 0) {
        s_flacCompressionRatio =
            (float)((s_flacValidSamples * 2) * s_metadataBlock->numChannels) /
            sbl;  // valid samples are 16 bit
        sbl = 0;
        s_flacBitrate = s_metadataBlock->sampleRate *
                        s_metadataBlock->bitsPerSample *
                        s_metadataBlock->numChannels;
        s_flacBitrate /= s_flacCompressionRatio;
        //      LOGE("s_flacBitrate %i, s_flacCompressionRatio %f,
        //     s_metadataBlock->sampleRate %i ", s_flacBitrate,
        //      s_flacCompressionRatio,s_metadataBlock->sampleRate);
      }
      if (s_offset != s_numOfOutSamples) return GIVE_NEXT_LOOP;
      if (s_offset > s_numOfOutSamples) {
        LOGE("offset has a wrong value");
      }
      s_offset = 0;
    }

    alignToByte();
    readUint(16, bytesLeft);

    //    s_flacCompressionRatio = (float)m_bytesDecoded /
    //    (float)s_numOfOutSamples *s_metadataBlock->numChannels * (16/8);
    //    log_i("s_flacCompressionRatio % f", s_flacCompressionRatio);
    s_flacStatus = DECODE_FRAME;
    return ERR_NONE;
  }

  int8_t flacDecodeFrame(uint8_t* inbuf, int32_t* bytesLeft) {
    if (specialIndexOf(inbuf, "OggS", *bytesLeft) ==
        0) {  // async? => new sync is OggS => reset and decode (not page 0 or
              // 1)
      DecoderReset();
      s_flacPageNr = 2;
      return OGG_SYNC_FOUND;
    }
    readUint(14 + 1, bytesLeft);  // synccode + reserved bit
    s_frameHeader->blockingStrategy = readUint(1, bytesLeft);
    s_frameHeader->blockSizeCode = readUint(4, bytesLeft);
    s_frameHeader->sampleRateCode = readUint(4, bytesLeft);
    s_frameHeader->chanAsgn = readUint(4, bytesLeft);
    s_frameHeader->sampleSizeCode = readUint(3, bytesLeft);
    if (!s_metadataBlock->numChannels) {
      if (s_frameHeader->chanAsgn == 0) s_metadataBlock->numChannels = 1;
      if (s_frameHeader->chanAsgn == 1) s_metadataBlock->numChannels = 2;
      if (s_frameHeader->chanAsgn > 7) s_metadataBlock->numChannels = 2;
    }
    if (s_metadataBlock->numChannels < 1) return ERR_UNKNOWN_CHANNEL_ASSIGNMENT;
    if (!s_metadataBlock->bitsPerSample) {
      if (s_frameHeader->sampleSizeCode == 1)
        s_metadataBlock->bitsPerSample = 8;
      if (s_frameHeader->sampleSizeCode == 2)
        s_metadataBlock->bitsPerSample = 12;
      if (s_frameHeader->sampleSizeCode == 4)
        s_metadataBlock->bitsPerSample = 16;
      if (s_frameHeader->sampleSizeCode == 5)
        s_metadataBlock->bitsPerSample = 20;
      if (s_frameHeader->sampleSizeCode == 6)
        s_metadataBlock->bitsPerSample = 24;
    }
    if (s_metadataBlock->bitsPerSample > 16) return ERR_BITS_PER_SAMPLE_TOO_BIG;
    if (s_metadataBlock->bitsPerSample < 8) return ERR_BITS_PER_SAMPLE_UNKNOWN;
    if (!s_metadataBlock->sampleRate) {
      if (s_frameHeader->sampleRateCode == 1)
        s_metadataBlock->sampleRate = 88200;
      if (s_frameHeader->sampleRateCode == 2)
        s_metadataBlock->sampleRate = 176400;
      if (s_frameHeader->sampleRateCode == 3)
        s_metadataBlock->sampleRate = 192000;
      if (s_frameHeader->sampleRateCode == 4)
        s_metadataBlock->sampleRate = 8000;
      if (s_frameHeader->sampleRateCode == 5)
        s_metadataBlock->sampleRate = 16000;
      if (s_frameHeader->sampleRateCode == 6)
        s_metadataBlock->sampleRate = 22050;
      if (s_frameHeader->sampleRateCode == 7)
        s_metadataBlock->sampleRate = 24000;
      if (s_frameHeader->sampleRateCode == 8)
        s_metadataBlock->sampleRate = 32000;
      if (s_frameHeader->sampleRateCode == 9)
        s_metadataBlock->sampleRate = 44100;
      if (s_frameHeader->sampleRateCode == 10)
        s_metadataBlock->sampleRate = 48000;
      if (s_frameHeader->sampleRateCode == 11)
        s_metadataBlock->sampleRate = 96000;
    }
    readUint(1, bytesLeft);
    uint32_t temp = (readUint(8, bytesLeft) << 24);
    temp = ~temp;
    uint32_t shift = 0x80000000;  // Number of leading zeros
    int8_t count = 0;
    for (int32_t i = 0; i < 32; i++) {
      if ((temp & shift) == 0) {
        count++;
        shift >>= 1;
      } else
        break;
    }
    count;
    for (int32_t i = 0; i < count; i++) readUint(8, bytesLeft);
    s_numOfOutSamples = 0;
    if (s_frameHeader->blockSizeCode == 1)
      s_numOfOutSamples = 192;
    else if (2 <= s_frameHeader->blockSizeCode &&
             s_frameHeader->blockSizeCode <= 5)
      s_numOfOutSamples = 576 << (s_frameHeader->blockSizeCode - 2);
    else if (s_frameHeader->blockSizeCode == 6)
      s_numOfOutSamples = readUint(8, bytesLeft) + 1;
    else if (s_frameHeader->blockSizeCode == 7)
      s_numOfOutSamples = readUint(16, bytesLeft) + 1;
    else if (8 <= s_frameHeader->blockSizeCode &&
             s_frameHeader->blockSizeCode <= 15)
      s_numOfOutSamples = 256 << (s_frameHeader->blockSizeCode - 8);
    else {
      return ERR_RESERVED_BLOCKSIZE_UNSUPPORTED;
    }
    if (s_numOfOutSamples > FLAC_MAX_OUTBUFFSIZE) {
      LOGE("Error: blockSizeOut too big ,%i bytes", s_numOfOutSamples);
      return ERR_BLOCKSIZE_TOO_BIG;
    }
    if (s_frameHeader->sampleRateCode == 12)
      readUint(8, bytesLeft);
    else if (s_frameHeader->sampleRateCode == 13 ||
             s_frameHeader->sampleRateCode == 14) {
      readUint(16, bytesLeft);
    }
    readUint(8, bytesLeft);
    s_flacStatus = DECODE_SUBFRAMES;
    return ERR_NONE;
  }

  uint16_t getOutputSamps() {
    int32_t vs = s_flacValidSamples;
    s_flacValidSamples = 0;
    return vs;
  }

  uint64_t getTotoalSamplesInStream() {
    if (!s_metadataBlock) return 0;
    return s_metadataBlock->totalSamples;
  }

  uint8_t getBitsPerSample() {
    if (!s_metadataBlock) return 0;
    return s_metadataBlock->bitsPerSample;
  }

  uint8_t getChannels() {
    if (!s_metadataBlock) return 0;
    return s_metadataBlock->numChannels;
  }

  uint32_t getSampRate() {
    if (!s_metadataBlock) return 0;
    return s_metadataBlock->sampleRate;
  }

  uint32_t getBitRate() { return s_flacBitrate; }

  uint32_t getAudioDataStart() { return s_flacAudioDataStart; }

  uint32_t getAudioFileDuration() {
    if (getSampRate()) {  // DIV0
      uint32_t afd =
          getTotoalSamplesInStream() / getSampRate();  // AudioFileDuration
      return afd;
    }
    return 0;
  }

  int8_t decodeSubframes(int32_t* bytesLeft) {
    if (s_frameHeader->chanAsgn <= 7) {
      for (int32_t ch = 0; ch < s_metadataBlock->numChannels; ch++)
        decodeSubframe(s_metadataBlock->bitsPerSample, ch, bytesLeft);
    } else if (8 <= s_frameHeader->chanAsgn && s_frameHeader->chanAsgn <= 10) {
      decodeSubframe(s_metadataBlock->bitsPerSample +
                         (s_frameHeader->chanAsgn == 9 ? 1 : 0),
                     0, bytesLeft);
      decodeSubframe(s_metadataBlock->bitsPerSample +
                         (s_frameHeader->chanAsgn == 9 ? 0 : 1),
                     1, bytesLeft);
      if (s_frameHeader->chanAsgn == 8) {
        for (int32_t i = 0; i < s_numOfOutSamples; i++)
          s_samplesBuffer[1][i] =
              (s_samplesBuffer[0][i] - s_samplesBuffer[1][i]);
      } else if (s_frameHeader->chanAsgn == 9) {
        for (int32_t i = 0; i < s_numOfOutSamples; i++)
          s_samplesBuffer[0][i] += s_samplesBuffer[1][i];
      } else if (s_frameHeader->chanAsgn == 10) {
        for (int32_t i = 0; i < s_numOfOutSamples; i++) {
          int32_t side = s_samplesBuffer[1][i];
          int32_t right = s_samplesBuffer[0][i] - (side >> 1);
          s_samplesBuffer[1][i] = right;
          s_samplesBuffer[0][i] = right + side;
        }
      } else {
        LOGE("unknown channel assignment, %i", s_frameHeader->chanAsgn);
        return ERR_UNKNOWN_CHANNEL_ASSIGNMENT;
      }
    } else {
      LOGE("Reserved channel assignment, %i", s_frameHeader->chanAsgn);
      return ERR_RESERVED_CHANNEL_ASSIGNMENT;
    }
    return ERR_NONE;
  }

  int8_t decodeSubframe(uint8_t sampleDepth, uint8_t ch, int32_t* bytesLeft) {
    int8_t ret = 0;
    readUint(
        1,
        bytesLeft);  // Zero bit padding, to prevent sync-fooling string of 1s
    uint8_t type = readUint(
        6, bytesLeft);  // Subframe type: 000000 : SUBFRAME_CONSTANT
                        //                000001 : SUBFRAME_VERBATIM
                        //                00001x : reserved
                        //                0001xx : reserved
                        //                001xxx : if(xxx <= 4) SUBFRAME_FIXED,
                        //                xxx=order ; else reserved 01xxxx :
                        //                reserved 1xxxxx : SUBFRAME_LPC,
                        //                xxxxx=order-1

    int32_t shift = readUint(
        1, bytesLeft);  // Wasted bits-per-sample' flag:
                        // 0 : no wasted bits-per-sample in source subblock, k=0
                        // 1 : k wasted bits-per-sample in source subblock, k-1
                        // follows, unary coded; e.g. k=3 => 001 follows, k=7 =>
                        // 0000001 follows.
    if (shift == 1) {
      while (readUint(1, bytesLeft) == 0) {
        shift++;
      }
    }
    sampleDepth -= shift;

    if (type == 0) {                                      // Constant coding
      int32_t s = readSignedInt(sampleDepth, bytesLeft);  // SUBFRAME_CONSTANT
      for (int32_t i = 0; i < s_numOfOutSamples; i++) {
        s_samplesBuffer[ch][i] = s;
      }
    } else if (type == 1) {  // Verbatim coding
      for (int32_t i = 0; i < s_numOfOutSamples; i++)
        s_samplesBuffer[ch][i] =
            readSignedInt(sampleDepth, bytesLeft);  // SUBFRAME_VERBATIM
    } else if (8 <= type && type <= 12) {
      ret = decodeFixedPredictionSubframe(type - 8, sampleDepth, ch,
                                          bytesLeft);  // SUBFRAME_FIXED
      if (ret) return ret;
    } else if (32 <= type && type <= 63) {
      ret = decodeLinearPredictiveCodingSubframe(type - 31, sampleDepth, ch,
                                                 bytesLeft);  // SUBFRAME_LPC
      if (ret) return ret;
    } else {
      return ERR_RESERVED_SUB_TYPE;
    }
    if (shift > 0) {
      for (int32_t i = 0; i < s_numOfOutSamples; i++) {
        s_samplesBuffer[ch][i] <<= shift;
      }
    }
    return ERR_NONE;
  }

  int8_t decodeFixedPredictionSubframe(uint8_t predOrder, uint8_t sampleDepth,
                                       uint8_t ch,
                                       int32_t* bytesLeft) {  // SUBFRAME_FIXED

    uint8_t ret = 0;
    for (uint8_t i = 0; i < predOrder; i++)
      s_samplesBuffer[ch][i] = readSignedInt(
          sampleDepth, bytesLeft);  // Unencoded warm-up samples (n = frame's
                                    // bits-per-sample * predictor order).
    ret = decodeResiduals(predOrder, ch, bytesLeft);
    if (ret) return ret;
    coefs.clear();
    coefs.shrink_to_fit();
    if (predOrder == 0) coefs.resize(0);
    if (predOrder == 1) coefs.push_back(1);  // FIXED_PREDICTION_COEFFICIENTS
    if (predOrder == 2) {
      coefs.push_back(2);
      coefs.push_back(-1);
    }
    if (predOrder == 3) {
      coefs.push_back(3);
      coefs.push_back(-3);
      coefs.push_back(1);
    }
    if (predOrder == 4) {
      coefs.push_back(4);
      coefs.push_back(-6);
      coefs.push_back(4);
      coefs.push_back(-1);
    }
    if (predOrder > 4) return ERR_PREORDER_TOO_BIG;  // Error: preorder > 4"
    restoreLinearPrediction(ch, 0);
    return ERR_NONE;
  }

  int8_t decodeLinearPredictiveCodingSubframe(int32_t lpcOrder,
                                              int32_t sampleDepth, uint8_t ch,
                                              int32_t* bytesLeft) {
    int8_t ret = 0;
    for (int32_t i = 0; i < lpcOrder; i++) {
      s_samplesBuffer[ch][i] = readSignedInt(
          sampleDepth, bytesLeft);  // Unencoded warm-up samples (n = frame's
                                    // bits-per-sample * lpc order).
    }
    int32_t precision = readUint(4, bytesLeft) +
                        1;  // (Quantized linear predictor coefficients'
                            // precision in bits)-1 (1111 = invalid).
    int32_t shift = readSignedInt(
        5,
        bytesLeft);  // Quantized linear predictor coefficient shift needed in
                     // bits (NOTE: this number is signed two's-complement).
    coefs.clear();
    coefs.shrink_to_fit();
    for (uint8_t i = 0; i < lpcOrder; i++) {
      coefs.push_back(readSignedInt(
          precision, bytesLeft));  // Unencoded predictor coefficients (n = qlp
                                   // coeff precision * lpc order) (NOTE: the
                                   // coefficients are signed two's-complement).
    }
    ret = decodeResiduals(lpcOrder, ch, bytesLeft);
    if (ret) return ret;
    restoreLinearPrediction(ch, shift);
    return ERR_NONE;
  }

  int8_t decodeResiduals(uint8_t warmup, uint8_t ch, int32_t* bytesLeft) {
    int32_t method = readUint(
        2, bytesLeft);  // Residual coding method:
                        // 00 : partitioned Rice coding with 4-bit Rice
                        // parameter; RESIDUAL_CODING_METHOD_PARTITIONED_RICE
                        // follows 01 : partitioned Rice coding with 5-bit Rice
                        // parameter; RESIDUAL_CODING_METHOD_PARTITIONED_RICE2
                        // follows 10-11 : reserved
    if (method >= 2) {
      return ERR_RESERVED_RESIDUAL_CODING;
    }
    uint8_t paramBits =
        method == 0 ? 4 : 5;  // RESIDUAL_CODING_METHOD_PARTITIONED_RICE ||
                              // RESIDUAL_CODING_METHOD_PARTITIONED_RICE2
    int32_t escapeParam = (method == 0 ? 0xF : 0x1F);
    int32_t partitionOrder = readUint(4, bytesLeft);  // Partition order
    int32_t numPartitions =
        1 << partitionOrder;  // There will be 2^order partitions.

    if (s_numOfOutSamples % numPartitions != 0) {
      return ERR_WRONG_RICE_PARTITION_NR;  // Error: Block size not
                                           // divisible by number of Rice
                                           // partitions
    }
    int32_t partitionSize = s_numOfOutSamples / numPartitions;

    for (int32_t i = 0; i < numPartitions; i++) {
      int32_t start = i * partitionSize + (i == 0 ? warmup : 0);
      int32_t end = (i + 1) * partitionSize;

      int32_t param = readUint(paramBits, bytesLeft);
      if (param < escapeParam) {
        for (int32_t j = start; j < end; j++) {
          if (s_f_bitReaderError) break;
          s_samplesBuffer[ch][j] = readRiceSignedInt(param, bytesLeft);
        }
      } else {
        int32_t numBits =
            readUint(5, bytesLeft);  // Escape code, meaning the partition is in
                                     // unencoded binary form using n bits per
                                     // sample; n follows as a 5-bit number.
        for (int32_t j = start; j < end; j++) {
          if (s_f_bitReaderError) break;
          s_samplesBuffer[ch][j] = readSignedInt(numBits, bytesLeft);
        }
      }
    }
    if (s_f_bitReaderError) return ERR_BITREADER_UNDERFLOW;
    return ERR_NONE;
  }

  void restoreLinearPrediction(uint8_t ch, uint8_t shift) {
    for (int32_t i = coefs.size(); i < s_numOfOutSamples; i++) {
      int32_t sum = 0;
      for (int32_t j = 0; j < coefs.size(); j++) {
        sum += s_samplesBuffer[ch][i - 1 - j] * coefs[j];
      }
      s_samplesBuffer[ch][i] += (sum >> shift);
    }
  }

  int32_t specialIndexOf(uint8_t* base, const char* str, int32_t baselen,
                         bool exact) {
    int32_t result = 0;  // seek for str in buffer or in header up to baselen,
                         // not nullterninated
    if (strlen(str) > baselen)
      return -1;  // if exact == true seekstr in buffer must have "\0" at the
                  // end
    for (int32_t i = 0; i < baselen - strlen(str); i++) {
      result = i;
      for (int32_t j = 0; j < strlen(str) + exact; j++) {
        if (*(base + i + j) != *(str + j)) {
          result = -1;
          break;
        }
      }
      if (result >= 0) break;
    }
    return result;
  }

  char* flac_x_ps_strdup(const char* str) {
    char* ps_str = NULL;
#if defined(ESP32)
    if (isPSRAMAvailable()) {
      ps_str = (char*)ps_calloc(strlen(str) + 1, sizeof(char));
    } else {
      ps_str = (char*)calloc(strlen(str) + 1, sizeof(char));
    }
#else
    ps_str = (char*)calloc(strlen(str) + 1, sizeof(char));
#endif
    strcpy(ps_str, str);
    return ps_str;
  }

  char* flac_x_ps_strndup(const char* str, uint16_t n) {
    char* ps_str = NULL;
#if defined(ESP32)
    if (isPSRAMAvailable()) {
      ps_str = (char*)ps_calloc(n + 1, sizeof(char));
    } else {
      ps_str = (char*)calloc(n + 1, sizeof(char));
    }
#else
    ps_str = (char*)calloc(n + 1, sizeof(char));
#endif
    strncpy(ps_str, str, n);
    return ps_str;
  }
};

}  // namespace audio_tools