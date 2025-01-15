
#pragma once
// #pragma GCC optimize ("O3")
// #pragma GCC diagnostic ignored "-Wnarrowing"

/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE Ogg SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Ogg SOURCE CODE IS (C) COPYRIGHT 1994-2007             *
 * by the Xiph.Org Foundation https://xiph.org/                     *
 *                                                                  *
 ********************************************************************/
/*
 * decoder.h
 * based on Xiph.Org Foundation vorbis decoder
 * adapted for the ESP32 by schreibfaul1
 *
 *  Created on: 13.02.2023
 *  Updated on: 03.04.2024
 */

#include <assert.h>

#include <vector>

#include "Arduino.h"
#include "native.h"

#define VI_FLOORB 2
#define VIF_POSIT 63

#define LSP_FRACBITS 14

#define OV_EREAD -128
#define OV_EFAULT -129
#define OV_EIMPL -130
#define OV_EINVAL -131
#define OV_ENOT -132
#define OV_EBADHEADER -133
#define OV_EVERSION -134
#define OV_ENOTAUDIO -135
#define OV_EBADPACKET -136
#define OV_EBADLINK -137
#define OV_ENOSEEK -138

#define INVSQ_LOOKUP_I_SHIFT 10
#define INVSQ_LOOKUP_I_MASK 1023
#define COS_LOOKUP_I_SHIFT 9
#define COS_LOOKUP_I_MASK 511
#define COS_LOOKUP_I_SZ 128

#define cPI3_8 (0x30fbc54d)
#define cPI2_8 (0x5a82799a)
#define cPI1_8 (0x7641af3d)

namespace audio_tools {

using namespace std;

enum : int8_t {
  CONTINUE = 110,
  PARSE_OGG_DONE = 100,
  ERR_NONE = 0,
  ERR_CHANNELS_OUT_OF_RANGE = -1,
  ERR_INVALID_SAMPLERATE = -2,
  ERR_EXTRA_CHANNELS_UNSUPPORTED = -3,
  ERR_DECODER_ASYNC = -4,
  ERR_OGG_SYNC_NOT_FOUND = -5,
  ERR_BAD_HEADER = -6,
  ERR_NOT_AUDIO = -7,
  ERR_BAD_PACKET = -8
};

struct codebook_t {
  uint8_t dim;           /* codebook dimensions (elements per vector) */
  int16_t entries;       /* codebook entries */
  uint16_t used_entries; /* populated codebook entries */
  uint32_t dec_maxlength;
  void *dec_table;
  uint32_t dec_nodeb;
  uint32_t dec_leafw;
  uint32_t dec_type; /* 0 = entry number
                        1 = packed vector of values
                        2 = packed vector of column offsets, maptype 1
                        3 = scalar offset into value array,  maptype 2  */
  int32_t q_min;
  int32_t q_minp;
  int32_t q_del;
  int32_t q_delp;
  int32_t q_seq;
  int32_t q_bits;
  uint8_t q_pack;
  void *q_val;
};

struct floor1class_t {
  char class_dim;           /* 1 to 8 */
  char class_subs;          /* 0,1,2,3 (bits: 1<<n poss) */
  uint8_t class_book;       /* subs ^ dim entries */
  uint8_t class_subbook[8]; /* [VIF_CLASS][subs] */
};

struct info_floor_t {
  int32_t order;
  int32_t rate;
  int32_t barkmap;
  int32_t ampbits;
  int32_t ampdB;
  int32_t numbooks; /* <= 16 */
  char books[16];
  floor1class_t *_class;   /* [VIF_CLASS] */
  uint8_t *partitionclass; /* [VIF_PARTS]; 0 to 15 */
  uint16_t *postlist;      /* [VIF_POSIT+2]; first two implicit */
  uint8_t *forward_index;  /* [VIF_POSIT+2]; */
  uint8_t *hineighbor;     /* [VIF_POSIT]; */
  uint8_t *loneighbor;     /* [VIF_POSIT]; */
  int32_t partitions;      /* 0 to 31 */
  int32_t posts;
  int32_t mult; /* 1 2 3 or 4 */
};

struct info_residue_t {
  int32_t type;
  uint8_t *stagemasks;
  uint8_t *stagebooks;
  /* block-partitioned VQ coded straight residue */
  uint32_t begin;
  uint32_t end;
  /* first stage (lossless partitioning) */
  uint32_t grouping; /* group n vectors per partition */
  char partitions;   /* possible codebooks for a partition */
  uint8_t groupbook; /* huffbook for partitioning */
  char stages;
};

struct submap_t {
  char floor;
  char residue;
};

struct coupling_step_t {  // Mapping backend generic
  uint8_t mag;
  uint8_t ang;
};

struct info_mapping_t {
  int32_t submaps;
  uint8_t *chmuxlist;
  submap_t *submaplist;
  int32_t coupling_steps;
  coupling_step_t *coupling;
};

struct info_mode_t {  // mode
  uint8_t blockflag;
  uint8_t mapping;
};

struct dsp_state_t {  // dsp_state buffers the current
                      // vorbis audio analysis/synthesis state.
  //    info     *vi;  // The DSP state be int32_ts to a specific logical
  //    bitstream oggpack_buffer_t opb;
  int32_t **work;
  int32_t **mdctright;
  int32_t out_begin;
  int32_t out_end;
  int32_t lW;  // last window
  uint32_t W;  // Window
};

struct bitReader_t {
  uint8_t *data;
  uint8_t length;
  uint16_t headbit;
  uint8_t *headptr;
  int32_t headend;
};

union magic {
  struct {
    int32_t lo;
    int32_t hi;
  } halves;
  int64_t whole;
};

class DecoderVorbisNative : public DecoderNative {
 protected:
  // ogg impl
  // global vars
  bool s_f_vorbisNewSteamTitle = false;  // streamTitle
  bool s_f_vorbisNewMetadataBlockPicture = false;
  bool s_f_oggFirstPage = false;
  bool s_f_oggContinuedPage = false;
  bool s_f_oggLastPage = false;
  bool s_f_parseOggDone = true;
  bool s_f_lastSegmentTable = false;
  bool s_f_vorbisStr_found = false;
  uint16_t s_identificatonHeaderLength = 0;
  uint16_t s_vorbisCommentHeaderLength = 0;
  uint16_t s_setupHeaderLength = 0;
  uint8_t s_pageNr = 0;
  uint16_t s_oggHeaderSize = 0;
  uint8_t s_vorbisChannels = 0;
  uint16_t s_vorbisSamplerate = 0;
  uint16_t s_lastSegmentTableLen = 0;
  uint8_t *s_lastSegmentTable = NULL;
  uint32_t s_vorbisBitRate = 0;
  uint32_t s_vorbisSegmentLength = 0;
  uint32_t s_vorbisBlockPicLenUntilFrameEnd = 0;
  uint32_t s_vorbisCurrentFilePos = 0;
  uint32_t s_vorbisAudioDataStart = 0;
  char *s_vorbisChbuf = NULL;
  int32_t s_vorbisValidSamples = 0;
  int32_t s_commentBlockSegmentSize = 0;
  uint8_t s_vorbisOldMode = 0;
  uint32_t s_blocksizes[2];
  uint32_t s_vorbisBlockPicPos = 0;
  uint32_t s_vorbisBlockPicLen = 0;
  int32_t s_vorbisRemainBlockPicLen = 0;
  int32_t s_commentLength = 0;

  uint8_t s_nrOfCodebooks = 0;
  uint8_t s_nrOfFloors = 0;
  uint8_t s_nrOfResidues = 0;
  uint8_t s_nrOfMaps = 0;
  uint8_t s_nrOfModes = 0;

  uint16_t *s_vorbisSegmentTable = NULL;
  uint16_t s_oggPage3Len = 0;  // length of the current audio segment
  uint8_t s_vorbisSegmentTableSize = 0;
  int16_t s_vorbisSegmentTableRdPtr = -1;
  int8_t s_vorbisError = 0;
  float s_vorbisCompressionRatio = 0;

  bitReader_t s_bitReader;

  codebook_t *s_codebooks = NULL;
  info_floor_t **s_floor_param = NULL;
  int8_t *s_floor_type = NULL;
  info_residue_t *s_residue_param = NULL;
  info_mapping_t *s_map_param = NULL;
  info_mode_t *s_mode_param = NULL;
  dsp_state_t *s_dsp_state = NULL;

  vector<uint32_t> s_vorbisBlockPicItem;
  static const int32_t FLOOR_fromdB_LOOKUP[256];
  static const int32_t vwin128[64];
  static const int32_t vwin256[128];
  static const int32_t vwin512[256];
  static const int32_t vwin1024[512];
  static const int32_t vwin2048[1024];
  static const int32_t vwin4096[2048];
  static const int32_t vwin8192[4096];
  static const int32_t sincos_lookup0[1026];
  static const int32_t sincos_lookup1[1024];
  static const int32_t INVSQ_LOOKUP_I[64 + 1];
  static const int32_t INVSQ_LOOKUP_IDel[64];
  static const int32_t COS_LOOKUP_I[COS_LOOKUP_I_SZ + 1];

  inline int32_t MULT32(int32_t x, int32_t y) {
    union magic magic;
    magic.whole = (int64_t)x * y;
    return magic.halves.hi;
  }

  inline int32_t MULT31_SHIFT15(int32_t x, int32_t y) {
    union magic magic;
    magic.whole = (int64_t)x * y;
    return ((uint32_t)(magic.halves.lo) >> 15) | ((magic.halves.hi) << 17);
  }

  inline int32_t MULT31(int32_t x, int32_t y) { return MULT32(x, y) << 1; }

  inline void XPROD31(int32_t a, int32_t b, int32_t t, int32_t v, int32_t *x,
                      int32_t *y) {
    *x = MULT31(a, t) + MULT31(b, v);
    *y = MULT31(b, t) - MULT31(a, v);
  }

  inline void XNPROD31(int32_t a, int32_t b, int32_t t, int32_t v, int32_t *x,
                       int32_t *y) {
    *x = MULT31(a, t) - MULT31(b, v);
    *y = MULT31(b, t) + MULT31(a, v);
  }

  inline int32_t CLIP_TO_15(int32_t x) {
    int32_t ret = x;
    ret -= ((x <= 32767) - 1) & (x - 32767);
    ret -= ((x >= -32768) - 1) & (x + 32768);
    return (ret);
  }

  bool allocateBuffers() {
    s_vorbisSegmentTable =
        (uint16_t *)decoder_calloc(256, sizeof(uint16_t));
    s_vorbisChbuf = (char *)decoder_calloc(256, sizeof(char));
    s_lastSegmentTable = (uint8_t *)decoder_malloc(4096);
    setDefaults();
    return true;
  }
  void freeBuffers() {
    if (s_vorbisSegmentTable) {
      free(s_vorbisSegmentTable);
      s_vorbisSegmentTable = NULL;
    }
    if (s_vorbisChbuf) {
      free(s_vorbisChbuf);
      s_vorbisChbuf = NULL;
    }
    if (s_lastSegmentTable) {
      free(s_lastSegmentTable);
      s_lastSegmentTable = NULL;
    }

    clearGlobalConfigurations();
  }
  void clearBuffers() {
    if (s_vorbisChbuf) memset(s_vorbisChbuf, 0, 256);
    bitReader_clear();
    if (s_lastSegmentTable) memset(s_lastSegmentTable, 0, 4096);
    if (s_vorbisSegmentTable) memset(s_vorbisSegmentTable, 0, 256);
    s_vorbisSegmentTableSize = 0;
    s_vorbisSegmentTableRdPtr = -1;
  }
  void setDefaults() {
    s_pageNr = 0;
    s_f_vorbisNewSteamTitle = false;  // streamTitle
    s_f_vorbisNewMetadataBlockPicture = false;
    s_f_lastSegmentTable = false;
    s_f_parseOggDone = false;
    s_f_oggFirstPage = false;
    s_f_oggContinuedPage = false;
    s_f_oggLastPage = false;
    s_f_vorbisStr_found = false;
    if (s_dsp_state) {
      dsp_destroy(s_dsp_state);
      s_dsp_state = NULL;
    }
    s_vorbisChannels = 0;
    s_vorbisSamplerate = 0;
    s_vorbisBitRate = 0;
    s_vorbisSegmentLength = 0;
    s_vorbisValidSamples = 0;
    s_vorbisSegmentTableSize = 0;
    s_vorbisCurrentFilePos = 0;
    s_vorbisAudioDataStart = 0;
    s_vorbisOldMode = 0xFF;
    s_vorbisSegmentTableRdPtr = -1;
    s_vorbisError = 0;
    s_lastSegmentTableLen = 0;
    s_vorbisBlockPicPos = 0;
    s_vorbisBlockPicLen = 0;
    s_vorbisBlockPicLenUntilFrameEnd = 0;
    s_commentBlockSegmentSize = 0;
    s_vorbisBlockPicItem.clear();
    s_vorbisBlockPicItem.shrink_to_fit();

    clearBuffers();
  }

  void clearGlobalConfigurations() {  // mode, mapping, floor etc
    if (s_nrOfCodebooks) {  // if we have a stream with changing codebooks,
                            // delete the old one
      for (int32_t i = 0; i < s_nrOfCodebooks; i++) {
        book_clear(s_codebooks + i);
      }
      s_nrOfCodebooks = 0;
    }
    if (s_codebooks) {
      free(s_codebooks);
      s_codebooks = NULL;
    }
    if (s_dsp_state) {
      dsp_destroy(s_dsp_state);
      s_dsp_state = NULL;
    }
    if (s_nrOfFloors) {
      for (int32_t i = 0; i < s_nrOfFloors; i++)
        floor_free_info(s_floor_param[i]);
      free(s_floor_param);
      s_nrOfFloors = 0;
    }
    if (s_nrOfResidues) {
      for (int32_t i = 0; i < s_nrOfResidues; i++)
        res_clear_info(s_residue_param + i);
      s_nrOfResidues = 0;
    }
    if (s_nrOfMaps) {
      for (int32_t i = 0; i < s_nrOfMaps; i++) {
        mapping_clear_info(s_map_param + i);
      }
      s_nrOfMaps = 0;
    }
    if (s_floor_type) {
      free(s_floor_type);
      s_floor_type = NULL;
    }
    if (s_residue_param) {
      free(s_residue_param);
      s_residue_param = NULL;
    }
    if (s_map_param) {
      free(s_map_param);
      s_map_param = NULL;
    }
    if (s_mode_param) {
      free(s_mode_param);
      s_mode_param = NULL;
    }
  }

  int32_t decode(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf) {
    int32_t ret = 0;
    int32_t segmentLength = 0;

    if (s_commentBlockSegmentSize) {
      if (s_commentBlockSegmentSize > 8192) {
        s_commentBlockSegmentSize -= 8192;
        s_commentLength -= 8192;
        *bytesLeft -= 8192;
        s_vorbisCurrentFilePos += 8192;
      } else {
        *bytesLeft -= s_commentBlockSegmentSize;
        s_vorbisCurrentFilePos += s_commentBlockSegmentSize;
        s_commentLength -= s_commentBlockSegmentSize;
        s_commentBlockSegmentSize = 0;
      }
      if (s_vorbisRemainBlockPicLen <= 0 &&
          !s_f_vorbisNewMetadataBlockPicture) {
        if (s_vorbisBlockPicItem.size() > 0) {  // get blockpic data
          // LOGI("-");
          // LOGI("metadata blockpic found at pos %i, size %i bytes",
          // s_vorbisBlockPicPos, s_vorbisBlockPicLen); for(int32_t i = 0; i <
          // s_vorbisBlockPicItem.size(); i += 2) { LOGI("segment %02i, pos
          // %07i, len %05i", i / 2, s_vorbisBlockPicItem[i],
          // s_vorbisBlockPicItem[i + 1]); }
          // LOGI("-");
          s_f_vorbisNewMetadataBlockPicture = true;
        }
      }
      return PARSE_OGG_DONE;
    }

    if (!s_vorbisSegmentTableSize) {
      s_vorbisSegmentTableRdPtr = -1;  // back to the parking position
      ret = parseOGG(inbuf, bytesLeft);
      s_f_parseOggDone = true;
      if (!s_vorbisSegmentTableSize) {
        LOGW("OggS without segments?");
      }
      return ret;
    }

    // With the last segment of a table, we don't know whether it will be
    // continued in the next Ogg page. So the last segment is saved.
    // s_lastSegmentTableLen specifies the size of the last saved segment. If
    // the next Ogg Page does not contain a 'continuedPage', the last segment is
    // played first. However, if 'continuedPage' is set, the first segment of
    // the new page is added to the saved segment and played.
    if (!s_lastSegmentTableLen) {
      if (s_vorbisSegmentTableSize) {
        s_vorbisSegmentTableRdPtr++;
        s_vorbisSegmentTableSize;
        segmentLength = s_vorbisSegmentTable[s_vorbisSegmentTableRdPtr];
      }
    }

    if (s_pageNr < 4)
      if (specialIndexOf(inbuf, "vorbis", 10) == 1) s_pageNr++;

    switch (s_pageNr) {
      case 0:
        ret = PARSE_OGG_DONE;  // do nothing
        break;
      case 1:
        ret = vorbisDecodePage1(
            inbuf, bytesLeft,
            segmentLength);  // blocksize, channels, samplerates
        break;
      case 2:
        ret = vorbisDecodePage2(inbuf, bytesLeft, segmentLength);  // comments
        break;
      case 3:
        ret = vorbisDecodePage3(inbuf, bytesLeft, segmentLength);  // codebooks
        break;
      case 4:
        ret = vorbisDecodePage4(inbuf, bytesLeft, segmentLength,
                                outbuf);  // decode audio
        break;
      default:
        LOGE("unknown page %s", s_pageNr);
        break;
    }
    return ret;
  }
  int32_t vorbisDecodePage1(uint8_t *inbuf, int32_t *bytesLeft,
                            uint32_t segmentLength) {
    int32_t ret = PARSE_OGG_DONE;
    clearGlobalConfigurations();  // if a new codebook is required, delete the
                                  // old one
    int32_t idx = specialIndexOf(inbuf, "vorbis", 10);
    if (idx == 1) {
      // LOGI("first packet (identification segmentLength) %i", segmentLength);
      s_identificatonHeaderLength = segmentLength;
      ret = parseFirstPacket(inbuf, segmentLength);
    } else {
      ret = ERR_NOT_AUDIO;  // #651
    }

    *bytesLeft -= segmentLength;
    s_vorbisCurrentFilePos += segmentLength;
    return ret;
  }
  int32_t vorbisDecodePage2(uint8_t *inbuf, int32_t *bytesLeft,
                            uint32_t segmentLength) {
    int32_t ret = PARSE_OGG_DONE;
    int32_t idx = specialIndexOf(inbuf, "vorbis", 10);
    if (idx == 1) {
      s_vorbisBlockPicItem.clear();
      s_vorbisBlockPicItem.shrink_to_fit();
      s_f_vorbisStr_found = true;
      s_vorbisCommentHeaderLength = segmentLength;
      ret = parseComment(inbuf, segmentLength);
      s_commentBlockSegmentSize = segmentLength;
      int32_t pLen =
          min((int32_t)s_vorbisBlockPicLen, s_vorbisBlockPicLenUntilFrameEnd);
      if (s_vorbisBlockPicLen && pLen > 0) {
        s_vorbisBlockPicItem.push_back(s_vorbisBlockPicPos);
        s_vorbisBlockPicItem.push_back(pLen);
      }
      s_vorbisRemainBlockPicLen = s_vorbisBlockPicLen - pLen;
      ret = PARSE_OGG_DONE;
    } else {
      s_commentBlockSegmentSize = segmentLength;
      uint32_t pLen = min(s_vorbisRemainBlockPicLen, (int32_t)segmentLength);
      if (s_vorbisRemainBlockPicLen && pLen > 0) {
        s_vorbisBlockPicItem.push_back(s_vorbisCurrentFilePos);
        s_vorbisBlockPicItem.push_back(pLen);
      }
      s_vorbisRemainBlockPicLen -= segmentLength;
      ret = PARSE_OGG_DONE;
    }
    return ret;
  }
  int32_t vorbisDecodePage3(uint8_t *inbuf, int32_t *bytesLeft,
                            uint32_t segmentLength) {
    int32_t ret = PARSE_OGG_DONE;
    int32_t idx = specialIndexOf(inbuf, "vorbis", 10);
    s_oggPage3Len = segmentLength;
    if (idx == 1) {
      // LOGI("third packet (setup segmentLength) %i", segmentLength);
      s_setupHeaderLength = segmentLength;
      bitReader_setData(inbuf, segmentLength);
      if (segmentLength == 4080) {
        // that is 16*255 bytes and thus the maximum segment size
        // it is possible that there is another block starting with 'OggS' in
        // which there is information about codebooks. It is possible that there
        // is another block starting with 'OggS' in which there is information
        // about codebooks.
        int32_t l = continuedOggPackets(inbuf + s_oggPage3Len);
        *bytesLeft -= l;
        s_vorbisCurrentFilePos += l;
        s_oggPage3Len += l;
        s_setupHeaderLength += l;
        bitReader_setData(inbuf, s_oggPage3Len);
        LOGW("s_oggPage3Len %i", s_oggPage3Len);
        s_pageNr++;
      }
      ret = parseCodebook();
    } else {
      LOGE("no \"vorbis\" something went wrong %i", segmentLength);
    }
    s_pageNr = 4;
    s_dsp_state = dsp_create();

    *bytesLeft -= segmentLength;
    s_vorbisCurrentFilePos += segmentLength;
    return ret;
  }
  int32_t vorbisDecodePage4(uint8_t *inbuf, int32_t *bytesLeft,
                            uint32_t segmentLength, int16_t *outbuf) {
    if (s_vorbisAudioDataStart == 0) {
      s_vorbisAudioDataStart = s_vorbisCurrentFilePos;
    }

    int32_t ret = 0;
    if (s_f_parseOggDone) {  // first loop after parseOGG()
      if (s_f_oggContinuedPage) {
        if (s_lastSegmentTableLen > 0 || segmentLength > 0) {
          if (s_lastSegmentTableLen + segmentLength > 1024)
            LOGE("continued page too big");
          memcpy(s_lastSegmentTable + s_lastSegmentTableLen, inbuf,
                 segmentLength);
          bitReader_setData(s_lastSegmentTable,
                            s_lastSegmentTableLen + segmentLength);
          ret = dsp_synthesis(s_lastSegmentTable,
                              s_lastSegmentTableLen + segmentLength, outbuf);
          uint16_t outBuffSize = 2048 * 2;
          s_vorbisValidSamples = dsp_pcmout(outbuf, outBuffSize);
          s_lastSegmentTableLen = 0;
          if (!ret && !segmentLength) ret = CONTINUE;
        } else {  // s_lastSegmentTableLen is 0 and segmentLength is 0
          s_vorbisValidSamples = 0;
          ret = CONTINUE;
        }
        s_f_oggContinuedPage = false;
      } else {  // last segment without continued Page
        if (s_lastSegmentTableLen) {
          bitReader_setData(s_lastSegmentTable, s_lastSegmentTableLen);
          ret =
              dsp_synthesis(s_lastSegmentTable, s_lastSegmentTableLen, outbuf);
          uint16_t outBuffSize = 2048 * 2;
          s_vorbisValidSamples = dsp_pcmout(outbuf, outBuffSize);
          s_lastSegmentTableLen = 0;
          if (ret == OV_ENOTAUDIO || ret == 0)
            ret = CONTINUE;  // if no error send continue
        } else {
          bitReader_setData(inbuf, segmentLength);
          ret = dsp_synthesis(inbuf, segmentLength, outbuf);
          uint16_t outBuffSize = 2048 * 2;
          s_vorbisValidSamples = dsp_pcmout(outbuf, outBuffSize);
          ret = 0;
        }
      }
    } else {  // not s_f_parseOggDone
      if (s_vorbisSegmentTableSize || s_f_lastSegmentTable) {
        // if(s_f_oggLastPage) LOGI("last page");
        bitReader_setData(inbuf, segmentLength);
        ret = dsp_synthesis(inbuf, segmentLength, outbuf);
        uint16_t outBuffSize = 2048 * 2;
        s_vorbisValidSamples = dsp_pcmout(outbuf, outBuffSize);
        ret = 0;
      } else {  // last segment
        if (segmentLength) {
          memcpy(s_lastSegmentTable, inbuf, segmentLength);
          s_lastSegmentTableLen = segmentLength;
          s_vorbisValidSamples = 0;
          ret = 0;
        } else {
          s_lastSegmentTableLen = 0;
          s_vorbisValidSamples = 0;
          ret = PARSE_OGG_DONE;
        }
      }
      s_f_oggFirstPage = false;
    }
    s_f_parseOggDone = false;
    if (s_f_oggLastPage && !s_vorbisSegmentTableSize) {
      setDefaults();
    }

    if (ret != CONTINUE) {  // nothing to do here, is playing from
                            // lastSegmentBuff
      *bytesLeft -= segmentLength;
      s_vorbisCurrentFilePos += segmentLength;
    }
    return ret;
  }

  uint8_t getChannels() { return s_vorbisChannels; }
  uint32_t getSampRate() { return s_vorbisSamplerate; }
  uint8_t getBitsPerSample() { return 16; }
  uint32_t getBitRate() { return s_vorbisBitRate; }
  uint32_t getAudioDataStart() { return s_vorbisAudioDataStart; }
  uint16_t getOutputSamps() {
    return s_vorbisValidSamples;  // 1024
  }
  char *getStreamTitle() {
    if (s_f_vorbisNewSteamTitle) {
      s_f_vorbisNewSteamTitle = false;
      return s_vorbisChbuf;
    }
    return NULL;
  }
  vector<uint32_t> getMetadataBlockPicture() {
    if (s_f_vorbisNewMetadataBlockPicture) {
      s_f_vorbisNewMetadataBlockPicture = false;
      return s_vorbisBlockPicItem;
    }
    if (s_vorbisBlockPicItem.size() > 0) {
      s_vorbisBlockPicItem.clear();
      s_vorbisBlockPicItem.shrink_to_fit();
    }
    return s_vorbisBlockPicItem;
  }

  int32_t parseFirstPacket(uint8_t *inbuf,
                           int16_t nBytes) {  // 4.2.2. Identification header
    // https://xiph.org/vorbis/doc/_I_spec.html#x1-820005
    // first bytes are: '.vorbis'
    uint16_t pos = 7;
    uint32_t version = *(inbuf + pos);
    version += *(inbuf + pos + 1) << 8;
    version += *(inbuf + pos + 2) << 16;
    version += *(inbuf + pos + 3) << 24;
    (void)version;

    uint8_t channels = *(inbuf + pos + 4);

    uint32_t sampleRate = *(inbuf + pos + 5);
    sampleRate += *(inbuf + pos + 6) << 8;
    sampleRate += *(inbuf + pos + 7) << 16;
    sampleRate += *(inbuf + pos + 8) << 24;

    uint32_t br_max = *(inbuf + pos + 9);
    br_max += *(inbuf + pos + 10) << 8;
    br_max += *(inbuf + pos + 11) << 16;
    br_max += *(inbuf + pos + 12) << 24;

    uint32_t br_nominal = *(inbuf + pos + 13);
    br_nominal += *(inbuf + pos + 14) << 8;
    br_nominal += *(inbuf + pos + 15) << 16;
    br_nominal += *(inbuf + pos + 16) << 24;

    uint32_t br_min = *(inbuf + pos + 17);
    br_min += *(inbuf + pos + 18) << 8;
    br_min += *(inbuf + pos + 19) << 16;
    br_min += *(inbuf + pos + 20) << 24;

    uint8_t blocksize = *(inbuf + pos + 21);

    s_blocksizes[0] = 1 << (blocksize & 0x0F);
    s_blocksizes[1] = 1 << ((blocksize & 0xF0) >> 4);

    if (s_blocksizes[0] < 64) {
      LOGE("blocksize[0] too low");
      return -1;
    }
    if (s_blocksizes[1] < s_blocksizes[0]) {
      LOGE("s_blocksizes[1] is smaller than s_blocksizes[0]");
      return -1;
    }
    if (s_blocksizes[1] > 8192) {
      LOGE("s_blocksizes[1] is too big");
      return -1;
    }

    if (channels < 1 || channels > 2) {
      LOGE("nr of channels is not valid ch=%i", channels);
      return -1;
    }
    s_vorbisChannels = channels;

    if (sampleRate < 4096 || sampleRate > 64000) {
      LOGE("sampleRate is not valid sr=%i", sampleRate);
      return -1;
    }
    s_vorbisSamplerate = sampleRate;

    s_vorbisBitRate = br_nominal;

    return PARSE_OGG_DONE;
  }

  int32_t parseComment(
      uint8_t *inbuf,
      int16_t nBytes) {  // reference https://xiph.org/vorbis/doc/v-comment.html

    // first bytes are: '.vorbis'
    uint32_t pos = 7;
    uint32_t vendorLength =
        *(inbuf + pos + 3)
        << 24;  // lengt of vendor string, e.g. Xiph.Org lib I 20070622
    vendorLength += *(inbuf + pos + 2) << 16;
    vendorLength += *(inbuf + pos + 1) << 8;
    vendorLength += *(inbuf + pos);

    if (vendorLength > 254) {  // guard
      LOGE("vorbis comment too long, vendorLength %i", vendorLength);
      return 0;
    }

    memcpy(s_vorbisChbuf, inbuf + 11, vendorLength);
    s_vorbisChbuf[vendorLength] = '\0';
    pos += 4 + vendorLength;
    s_vorbisCommentHeaderLength -= (7 + 4 + vendorLength);

    // LOGI("vendorLength %x", vendorLength);
    // LOGI("vendorString %s", s_vorbisChbuf);

    uint8_t nrOfComments = *(inbuf + pos);
    // LOGI("nrOfComments %i", nrOfComments);
    pos += 4;
    s_vorbisCommentHeaderLength -= 4;

    int32_t idx = 0;
    char *artist = NULL;
    char *title = NULL;
    uint32_t commentLength = 0;
    for (int32_t i = 0; i < nrOfComments; i++) {
      commentLength = 0;
      commentLength = *(inbuf + pos + 3) << 24;
      commentLength += *(inbuf + pos + 2) << 16;
      commentLength += *(inbuf + pos + 1) << 8;
      commentLength += *(inbuf + pos);
      s_commentLength = commentLength;

      uint8_t cl = min((uint32_t)254, commentLength);
      memcpy(s_vorbisChbuf, inbuf + pos + 4, cl);
      s_vorbisChbuf[cl] = '\0';

      // LOGI("commentLength %i comment %s", commentLength, s_vorbisChbuf);

      idx = specialIndexOf((uint8_t *)s_vorbisChbuf, "artist=", 10);
      if (idx != 0)
        idx = specialIndexOf((uint8_t *)s_vorbisChbuf, "ARTIST=", 10);
      if (idx == 0) {
        artist = strndup((const char *)(s_vorbisChbuf + 7), commentLength - 7);
        s_commentLength = 0;
      }

      idx = specialIndexOf((uint8_t *)s_vorbisChbuf, "title=", 10);
      if (idx != 0)
        idx = specialIndexOf((uint8_t *)s_vorbisChbuf, "TITLE=", 10);
      if (idx == 0) {
        title = strndup((const char *)(s_vorbisChbuf + 6), commentLength - 6);
        s_commentLength = 0;
      }

      idx = specialIndexOf((uint8_t *)s_vorbisChbuf,
                           "metadata_block_picture=", 25);
      if (idx != 0)
        idx = specialIndexOf((uint8_t *)s_vorbisChbuf, "METADATA_BLOCK_PICTURE",
                             25);
      if (idx == 0) {
        s_vorbisBlockPicLen = commentLength - 23;
        s_vorbisBlockPicPos += s_vorbisCurrentFilePos + 4 + pos + 23;
        s_vorbisBlockPicLenUntilFrameEnd = s_vorbisCommentHeaderLength - 4 - 23;
      }
      pos += commentLength + 4;
      s_vorbisCommentHeaderLength -= (4 + commentLength);
    }
    if (artist && title) {
      strcpy(s_vorbisChbuf, artist);
      strcat(s_vorbisChbuf, " - ");
      strcat(s_vorbisChbuf, title);
      s_f_vorbisNewSteamTitle = true;
    } else if (artist) {
      strcpy(s_vorbisChbuf, artist);
      s_f_vorbisNewSteamTitle = true;
    } else if (title) {
      strcpy(s_vorbisChbuf, title);
      s_f_vorbisNewSteamTitle = true;
    }
    if (artist) {
      free(artist);
      artist = NULL;
    }
    if (title) {
      free(title);
      title = NULL;
    }

    return PARSE_OGG_DONE;
  }

  int32_t parseCodebook() {
    s_bitReader.headptr += 7;
    s_bitReader.length = s_oggPage3Len;

    int32_t i;
    int32_t ret = 0;

    s_nrOfCodebooks = bitReader(8) + 1;
    s_codebooks = (codebook_t *)decoder_calloc(s_nrOfCodebooks,
                                                    sizeof(*s_codebooks));

    for (i = 0; i < s_nrOfCodebooks; i++) {
      ret = book_unpack(s_codebooks + i);
      if (ret) LOGE("codebook %i returned err", i);
      if (ret) goto err_out;
    }

    /* time backend settings, not actually used */
    i = bitReader(6);
    for (; i >= 0; i) {
      ret = bitReader(16);
      if (ret != 0) {
        LOGE("err while reading backend settings");
        goto err_out;
      }
    }
    /* floor backend settings */
    s_nrOfFloors = bitReader(6) + 1;

    s_floor_param = (info_floor_t **)decoder_malloc(
        sizeof(*s_floor_param) * s_nrOfFloors);
    s_floor_type = (int8_t *)decoder_malloc(sizeof(int8_t) * s_nrOfFloors);
    for (i = 0; i < s_nrOfFloors; i++) {
      s_floor_type[i] = bitReader(16);
      if (s_floor_type[i] < 0 || s_floor_type[i] >= VI_FLOORB) {
        LOGE("err while reading floors");
        goto err_out;
      }
      if (s_floor_type[i]) {
        s_floor_param[i] = floor1_info_unpack();
      } else {
        s_floor_param[i] = floor0_info_unpack();
      }
      if (!s_floor_param[i]) {
        LOGE("floor parameter not found");
        goto err_out;
      }
    }

    /* residue backend settings */
    s_nrOfResidues = bitReader(6) + 1;
    s_residue_param = (info_residue_t *)decoder_malloc(
        sizeof(*s_residue_param) * s_nrOfResidues);
    for (i = 0; i < s_nrOfResidues; i++) {
      if (res_unpack(s_residue_param + i)) {
        LOGE("err while unpacking residues");
        goto err_out;
      }
    }

    // /* map backend settings */
    s_nrOfMaps = bitReader(6) + 1;
    s_map_param = (info_mapping_t *)decoder_malloc(sizeof(*s_map_param) *
                                                        s_nrOfMaps);
    for (i = 0; i < s_nrOfMaps; i++) {
      if (bitReader(16) != 0) goto err_out;
      if (mapping_info_unpack(s_map_param + i)) {
        LOGE("err while unpacking mappings");
        goto err_out;
      }
    }

    /* mode settings */
    s_nrOfModes = bitReader(6) + 1;
    s_mode_param =
        (info_mode_t *)decoder_malloc(s_nrOfModes * sizeof(*s_mode_param));
    for (i = 0; i < s_nrOfModes; i++) {
      s_mode_param[i].blockflag = bitReader(1);
      if (bitReader(16)) goto err_out;
      if (bitReader(16)) goto err_out;
      s_mode_param[i].mapping = bitReader(8);
      if (s_mode_param[i].mapping >= s_nrOfMaps) {
        LOGE("too many modes");
        goto err_out;
      }
    }

    if (bitReader(1) != 1) {
      LOGE("codebooks, end bit not found");
      goto err_out;
    }
    // if(s_setupHeaderLength != s_bitReader.headptr - s_bitReader.data){
    //     LOGE("Error reading setup header, assumed %i bytes, read %i bytes",
    //     s_setupHeaderLength, s_bitReader.headptr - s_bitReader.data); goto
    //     err_out;
    // }
    /* top level EOP check */

    return PARSE_OGG_DONE;

  err_out:
    //    info_clear(vi);
    LOGE("err in codebook!  at pos %d", s_bitReader.headptr - s_bitReader.data);
    return (OV_EBADHEADER);
  }

  int32_t parseOGG(uint8_t *inbuf, int32_t *bytesLeft) {
    // reference https://www.xiph.org/ogg/doc/rfc3533.txt
    int32_t ret = 0;
    (void)ret;

    int32_t idx = specialIndexOf(inbuf, "OggS", 8192);
    if (idx != 0) {
      if (s_f_oggContinuedPage) return ERR_DECODER_ASYNC;
      inbuf += idx;
      *bytesLeft -= idx;
      s_vorbisCurrentFilePos += idx;
    }

    int16_t segmentTableWrPtr = -1;

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
    s_vorbisSegmentLength = 0;
    segmentTableWrPtr = -1;

    for (int32_t i = 0; i < pageSegments; i++) {
      int32_t n = *(inbuf + 27 + i);
      while (*(inbuf + 27 + i) == 255) {
        i++;
        if (i == pageSegments) break;
        n += *(inbuf + 27 + i);
      }
      segmentTableWrPtr++;
      s_vorbisSegmentTable[segmentTableWrPtr] = n;
      s_vorbisSegmentLength += n;
    }
    s_vorbisSegmentTableSize = segmentTableWrPtr + 1;
    s_vorbisCompressionRatio =
        (float)(960 * 2 * pageSegments) /
        s_vorbisSegmentLength;  // const 960 validBytes out

    bool continuedPage =
        headerType & 0x01;  // set: page contains data of a packet continued
                            // from the previous page
    bool firstPage =
        headerType &
        0x02;  // set: this is the first page of a logical bitstream (bos)
    bool lastPage =
        headerType &
        0x04;  // set: this is the last page of a logical bitstream (eos)

    uint16_t headerSize = pageSegments + 27;

    // LOGI("headerSize %i, s_vorbisSegmentLength %i, s_vorbisSegmentTableSize
    // %i", headerSize, s_vorbisSegmentLength, s_vorbisSegmentTableSize);
    if (firstPage || continuedPage || lastPage) {
      // LOGW("firstPage %i  continuedPage %i  lastPage %i", firstPage,
      // continuedPage, lastPage);
    }

    *bytesLeft -= headerSize;
    inbuf += headerSize;
    s_vorbisCurrentFilePos += headerSize;
    //   if(s_pageNr < 4 && !continuedPage) s_pageNr++;

    s_f_oggFirstPage = firstPage;
    s_f_oggContinuedPage = continuedPage;
    s_f_oggLastPage = lastPage;
    s_oggHeaderSize = headerSize;

    if (firstPage) s_pageNr = 0;

    return PARSE_OGG_DONE;  // no error
  }

  uint16_t continuedOggPackets(uint8_t *inbuf) {
    // skip OggS header to pageSegments
    // LOGW("%c%c%c%c", *(inbuf+0), *(inbuf+1), *(inbuf+2), *(inbuf+3));
    uint16_t segmentLength = 0;
    uint8_t headerType = *(inbuf + 5);
    uint8_t pageSegments =
        *(inbuf + 26);  // giving the number of segment entries

    for (int32_t i = 0; i < pageSegments; i++) {
      int32_t n = *(inbuf + 27 + i);
      while (*(inbuf + 27 + i) == 255) {
        i++;
        if (i == pageSegments) break;
        n += *(inbuf + 27 + i);
      }
      segmentLength += n;
    }
    uint16_t headerSize = pageSegments + 27;
    bool continuedPage = headerType & 0x01;

    if (continuedPage) {
      //  codebook data are in 2 ogg packets
      //  codebook data must no be interrupted by oggPH (ogg page header)
      //  therefore shift codebook data2 left (oggPH size times) whith memmove
      //  |oggPH| codebook data 1 |oggPH| codebook data 2 |oggPH|
      //  |oppPH| codebook data 1 + 2              |unused|occPH|
      LOGE("continued page");
      memmove(inbuf, inbuf + headerSize, segmentLength);
      return segmentLength + headerSize;
      return 0;
    }

    return 0;
  }

  int32_t FindSyncWord(unsigned char *buf, int32_t nBytes) {
    // assume we have a ogg wrapper
    int32_t idx = specialIndexOf(buf, "OggS", nBytes);
    if (idx >= 0) {  // Magic Word found
                     //    LOGI("OggS found at %i", idx);
      return idx;
    }
    // LOGI("find sync");
    return ERR_OGG_SYNC_NOT_FOUND;
  }
  //-
  int32_t book_unpack(codebook_t *s) {
    char *lengthlist = NULL;
    uint8_t quantvals = 0;
    int32_t i, j;
    int32_t maptype;
    int32_t ret = 0;

    memset(s, 0, sizeof(*s));

    /* make sure alignment is correct */
    if (bitReader(24) != 0x564342) {
      LOGE("String \"BCV\" not found");
      goto eofout;  // "BCV"
    }

    /* first the basic parameters */
    ret = bitReader(16);
    if (ret < 0) printf("error in book_unpack, ret =%li\n", (long int)ret);
    if (ret > 255) printf("error in book_unpack, ret =%li\n", (long int)ret);
    s->dim = (uint8_t)ret;
    s->entries = bitReader(24);
    if (s->entries == -1) {
      LOGE("no entries in unpack codebooks ?");
      goto eofout;
    }

    /* codeword ordering.... length ordered or unordered? */
    switch (bitReader(1)) {
      case 0:
        /* unordered */
        lengthlist =
            (char *)decoder_malloc(sizeof(*lengthlist) * s->entries);

        /* allocated but unused entries? */
        if (bitReader(1)) {
          /* yes, unused entries */

          for (i = 0; i < s->entries; i++) {
            if (bitReader(1)) {
              int32_t num = bitReader(5);
              if (num == -1) goto eofout;
              lengthlist[i] = num + 1;
              s->used_entries++;
              if (num + 1 > s->dec_maxlength) s->dec_maxlength = num + 1;
            } else
              lengthlist[i] = 0;
          }
        } else {
          /* all entries used; no tagging */
          s->used_entries = s->entries;
          for (i = 0; i < s->entries; i++) {
            int32_t num = bitReader(5);
            if (num == -1) goto eofout;
            lengthlist[i] = num + 1;

            if (num + 1 > s->dec_maxlength) s->dec_maxlength = num + 1;
          }
        }
        break;
      case 1:
        /* ordered */
        {
          int32_t length = bitReader(5) + 1;

          s->used_entries = s->entries;
          lengthlist =
              (char *)decoder_malloc(sizeof(*lengthlist) * s->entries);

          for (i = 0; i < s->entries;) {
            int32_t num = bitReader(ilog(s->entries - i));
            if (num == -1) goto eofout;
            for (j = 0; j < num && i < s->entries; j++, i++)
              lengthlist[i] = length;
            s->dec_maxlength = length;
            length++;
          }
        }
        break;
      default:
        /* EOF */
        goto eofout;
    }

    /* Do we have a mapping to unpack? */
    if ((maptype = bitReader(4)) > 0) {
      s->q_min = float32_unpack(bitReader(32), &s->q_minp);
      s->q_del = float32_unpack(bitReader(32), &s->q_delp);

      s->q_bits = bitReader(4) + 1;
      s->q_seq = bitReader(1);

      s->q_del >>= s->q_bits;
      s->q_delp += s->q_bits;
    }

    switch (maptype) {
      case 0:
        /* no mapping; decode type 0 */
        /* how many bytes for the indexing? */
        /* this is the correct boundary here; we lose one bit to node/leaf mark
         */
        s->dec_nodeb =
            determine_node_bytes(s->used_entries, ilog(s->entries) / 8 + 1);
        s->dec_leafw =
            determine_leaf_words(s->dec_nodeb, ilog(s->entries) / 8 + 1);
        s->dec_type = 0;
        ret = make_decode_table(s, lengthlist, quantvals, maptype);
        if (ret != 0) {
          goto errout;
        }
        break;

      case 1:
        /* mapping type 1; implicit values by lattice  position */
        quantvals = book_maptype1_quantvals(s);

        /* dec_type choices here are 1,2; 3 doesn't make sense */
        {
          /* packed values */
          int32_t total1 = (s->q_bits * s->dim + 8) / 8; /* remember flag bit */
          /* vector of column offsets; remember flag bit */
          int32_t total2 =
              (_ilog(quantvals - 1) * s->dim + 8) / 8 + (s->q_bits + 7) / 8;

          if (total1 <= 4 && total1 <= total2) {
            /* use dec_type 1: vector of packed values */
            /* need quantized values before  */
            s->q_val = _malloc_heap_psram(sizeof(uint16_t) * quantvals);
            for (i = 0; i < quantvals; i++)
              ((uint16_t *)s->q_val)[i] = bitReader(s->q_bits);

            if (oggpack_eop()) {
              if (s->q_val) {
                free(s->q_val), s->q_val = NULL;
              }
              goto eofout;
            }

            s->dec_type = 1;
            s->dec_nodeb = determine_node_bytes(s->used_entries,
                                                (s->q_bits * s->dim + 8) / 8);
            s->dec_leafw = determine_leaf_words(s->dec_nodeb,
                                                (s->q_bits * s->dim + 8) / 8);
            ret = make_decode_table(s, lengthlist, quantvals, maptype);
            if (ret) {
              if (s->q_val) {
                free(s->q_val), s->q_val = NULL;
              }
              goto errout;
            }

            if (s->q_val) {
              free(s->q_val), s->q_val = NULL;
            } /* about to go out of scope; make_decode_table was using it */
          } else {
            /* use dec_type 2: packed vector of column offsets */
            /* need quantized values before */
            if (s->q_bits <= 8) {
              s->q_val = _malloc_heap_psram(quantvals);
              for (i = 0; i < quantvals; i++)
                ((uint8_t *)s->q_val)[i] = bitReader(s->q_bits);
            } else {
              s->q_val = _malloc_heap_psram(quantvals * 2);
              for (i = 0; i < quantvals; i++)
                ((uint16_t *)s->q_val)[i] = bitReader(s->q_bits);
            }

            if (oggpack_eop()) goto eofout;

            s->q_pack = ilog(quantvals - 1);
            s->dec_type = 2;
            s->dec_nodeb = determine_node_bytes(
                s->used_entries, (_ilog(quantvals - 1) * s->dim + 8) / 8);
            s->dec_leafw = determine_leaf_words(
                s->dec_nodeb, (_ilog(quantvals - 1) * s->dim + 8) / 8);

            ret = make_decode_table(s, lengthlist, quantvals, maptype);
            if (ret) {
              goto errout;
            }
          }
        }
        break;
      case 2:
        /* mapping type 2; explicit array of values */
        quantvals = s->entries * s->dim;
        /* dec_type choices here are 1,3; 2 is not possible */

        if ((s->q_bits * s->dim + 8) / 8 <= 4) { /* remember flag bit */
          /* use dec_type 1: vector of packed values */

          s->dec_type = 1;
          s->dec_nodeb = determine_node_bytes(s->used_entries,
                                              (s->q_bits * s->dim + 8) / 8);
          s->dec_leafw =
              determine_leaf_words(s->dec_nodeb, (s->q_bits * s->dim + 8) / 8);
          if (_make_decode_table(s, lengthlist, quantvals, maptype))
            goto errout;
        } else {
          /* use dec_type 3: scalar offset into packed value array */

          s->dec_type = 3;
          s->dec_nodeb = determine_node_bytes(
              s->used_entries, ilog(s->used_entries - 1) / 8 + 1);
          s->dec_leafw = determine_leaf_words(
              s->dec_nodeb, ilog(s->used_entries - 1) / 8 + 1);
          if (_make_decode_table(s, lengthlist, quantvals, maptype))
            goto errout;

          /* get the vals & pack them */
          s->q_pack = (s->q_bits + 7) / 8 * s->dim;
          s->q_val = _malloc_heap_psram(s->q_pack * s->used_entries);

          if (s->q_bits <= 8) {
            for (i = 0; i < s->used_entries * s->dim; i++)
              ((uint8_t *)(s->q_val))[i] = bitReader(s->q_bits);
          } else {
            for (i = 0; i < s->used_entries * s->dim; i++)
              ((uint16_t *)(s->q_val))[i] = bitReader(s->q_bits);
          }
        }
        break;
      default:
        LOGE("maptype %i schould be 0, 1 or 2", maptype);
        goto errout;
    }
    if (oggpack_eop()) goto eofout;
    if (lengthlist) {
      free(lengthlist);
      lengthlist = NULL;
    }
    if (s->q_val) {
      free(s->q_val), s->q_val = NULL;
    }
    return 0;  // ok
  errout:
  eofout:
    book_clear(s);
    if (lengthlist) {
      free(lengthlist);
      lengthlist = NULL;
    }
    if (s->q_val) {
      free(s->q_val), s->q_val = NULL;
    }
    return -1;  // error
  }
  //-

  int32_t specialIndexOf(uint8_t *base, const char *str, int32_t baselen,
                         bool exact) {
    int32_t result = -1;  // seek for str in buffer or in header up to baselen,
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

  const uint32_t mask[] = {
      0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000f, 0x0000001f,
      0x0000003f, 0x0000007f, 0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
      0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff, 0x0001ffff,
      0x0003ffff, 0x0007ffff, 0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
      0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff, 0x1fffffff,
      0x3fffffff, 0x7fffffff, 0xffffffff};

  void bitReader_clear() {
    s_bitReader.data = NULL;
    s_bitReader.headptr = NULL;
    s_bitReader.length = 0;
    s_bitReader.headend = 0;
    s_bitReader.headbit = 0;
  }

  void bitReader_setData(uint8_t *buff, uint16_t buffSize) {
    s_bitReader.data = buff;
    s_bitReader.headptr = buff;
    s_bitReader.length = buffSize;
    s_bitReader.headend = buffSize * 8;
    s_bitReader.headbit = 0;
  }

  /* Read in bits without advancing the bitptr; bits <= 32 */
  int32_t bitReader_look(uint16_t nBits) {
    uint32_t m = mask[nBits];
    int32_t ret = 0;

    nBits += s_bitReader.headbit;

    if (nBits >= s_bitReader.headend << 3) {
      uint8_t *ptr = s_bitReader.headptr;
      if (nBits) {
        ret = *ptr++ >> s_bitReader.headbit;
        if (nBits > 8) {
          ret |= *ptr++ << (8 - s_bitReader.headbit);
          if (nBits > 16) {
            ret |= *ptr++ << (16 - s_bitReader.headbit);
            if (nBits > 24) {
              ret |= *ptr++ << (24 - s_bitReader.headbit);
              if (nBits > 32 && s_bitReader.headbit) {
                ret |= *ptr << (32 - s_bitReader.headbit);
              }
            }
          }
        }
      }
    } else {
      /* make this a switch jump-table */
      ret = s_bitReader.headptr[0] >> s_bitReader.headbit;
      if (nBits > 8) {
        ret |= s_bitReader.headptr[1] << (8 - s_bitReader.headbit);
        if (nBits > 16) {
          ret |= s_bitReader.headptr[2] << (16 - s_bitReader.headbit);
          if (nBits > 24) {
            ret |= s_bitReader.headptr[3] << (24 - s_bitReader.headbit);
            if (nBits > 32 && s_bitReader.headbit)
              ret |= s_bitReader.headptr[4] << (32 - s_bitReader.headbit);
          }
        }
      }
    }

    ret &= (int32_t)m;
    return ret;
  }

  /* bits <= 32 */
  int32_t bitReader(uint16_t nBits) {
    int32_t ret = bitReader_look(nBits);
    if (bitReader_adv(nBits) < 0) return -1;
    return (ret);
  }

  /* limited to 32 at a time */
  int8_t bitReader_adv(uint16_t nBits) {
    nBits += s_bitReader.headbit;
    s_bitReader.headbit = nBits & 7;
    s_bitReader.headend -= (nBits >> 3);
    s_bitReader.headptr += (nBits >> 3);
    if (s_bitReader.headend < 1) {
      return -1;
      LOGE("error in bitreader");
    }
    return 0;
  }
  //-
  int32_t ilog(uint32_t v) {
    int32_t ret = 0;
    if (v) v;
    while (v) {
      ret++;
      v >>= 1;
    }
    return (ret);
  }

  uint8_t ilog(uint32_t v) {
    uint8_t ret = 0;
    while (v) {
      ret++;
      v >>= 1;
    }
    return (ret);
  }
  //-
  /* 32 bit float (not IEEE; nonnormalized mantissa + biased exponent) :
   neeeeeee eeemmmmm mmmmmmmm mmmmmmmm Why not IEEE?  It's just not that
   important here. */

  int32_t float32_unpack(int32_t val, int32_t *point) {
    int32_t mant = val & 0x1fffff;
    bool sign = val < 0;

    *point = ((val & 0x7fe00000L) >> 21) - 788;

    if (mant) {
      while (!(mant & 0x40000000)) {
        mant <<= 1;
        *point -= 1;
      }
      if (sign) mant = -mant;
    } else {
      *point = -9999;
    }
    return mant;
  }
  //-
  /* choose the smallest supported node size that fits our decode table. Legal
   * bytewidths are 1/1 1/2 2/2 2/4 4/4 */
  int32_t determine_node_bytes(uint32_t used, uint8_t leafwidth) {
    /* special case small books to size 4 to avoid multiple special cases in
     * repack */
    if (used < 2) return 4;

    if (leafwidth == 3) leafwidth = 4;
    if (_ilog((3 * used - 6)) + 1 <= leafwidth * 4)
      return leafwidth / 2 ? leafwidth / 2 : 1;
    return leafwidth;
  }
  //-
  /* convenience/clarity; leaves are specified as multiple of node word size (1
   * or 2) */
  int32_t determine_leaf_words(int32_t nodeb, int32_t leafwidth) {
    if (leafwidth > nodeb) return 2;
    return 1;
  }
  //-
  int32_t make_decode_table(codebook_t *s, char *lengthlist, uint8_t quantvals,
                            int32_t maptype) {
    uint32_t *work = nullptr;

    if (s->dec_nodeb == 4) {
      s->dec_table =
          _malloc_heap_psram((s->used_entries * 2 + 1) * sizeof(*work));
      /* +1 (rather than -2) is to accommodate 0 and 1 sized books, which are
       * specialcased to nodeb==4 */
      if (_make_words(lengthlist, s->entries, (uint32_t *)s->dec_table,
                      quantvals, s, maptype))
        return 1;

      return 0;
    }

    work = (uint32_t *)decoder_calloc((uint32_t)(s->used_entries * 2 - 2),
                                           sizeof(*work));
    if (!work) LOGE("oom");

    if (_make_words(lengthlist, s->entries, work, quantvals, s, maptype)) {
      if (work) {
        free(work);
        work = NULL;
      }
      return 1;
    }
    s->dec_table = _malloc_heap_psram(
        (s->used_entries * (s->dec_leafw + 1) - 2) * s->dec_nodeb);
    if (s->dec_leafw == 1) {
      switch (s->dec_nodeb) {
        case 1:
          for (uint32_t i = 0; i < s->used_entries * 2 - 2; i++)
            ((uint8_t *)s->dec_table)[i] =
                (uint16_t)((work[i] & 0x80000000UL) >> 24) | work[i];
          break;
        case 2:
          for (uint32_t i = 0; i < s->used_entries * 2 - 2; i++)
            ((uint16_t *)s->dec_table)[i] =
                (uint16_t)((work[i] & 0x80000000UL) >> 16) | work[i];
          break;
      }
    } else {
      /* more complex; we have to do a two-pass repack that updates the node
       * indexing. */
      uint32_t top = s->used_entries * 3 - 2;
      if (s->dec_nodeb == 1) {
        uint8_t *out = (uint8_t *)s->dec_table;

        for (int32_t i = s->used_entries * 2 - 4; i >= 0; i -= 2) {
          if (work[i] & 0x80000000UL) {
            if (work[i + 1] & 0x80000000UL) {
              top -= 4;
              out[top] = (uint8_t)(work[i] >> 8 & 0x7f) | 0x80;
              out[top + 1] = (uint8_t)(work[i + 1] >> 8 & 0x7f) | 0x80;
              out[top + 2] = (uint8_t)work[i] & 0xff;
              out[top + 3] = (uint8_t)work[i + 1] & 0xff;
            } else {
              top -= 3;
              out[top] = (uint8_t)(work[i] >> 8 & 0x7f) | 0x80;
              out[top + 1] = (uint8_t)work[work[i + 1] * 2];
              out[top + 2] = (uint8_t)work[i] & 0xff;
            }
          } else {
            if (work[i + 1] & 0x80000000UL) {
              top -= 3;
              out[top] = (uint8_t)work[work[i] * 2];
              out[top + 1] = (uint8_t)(work[i + 1] >> 8 & 0x7f) | 0x80;
              out[top + 2] = (uint8_t)work[i + 1] & 0xff;
            } else {
              top -= 2;
              out[top] = (uint8_t)work[work[i] * 2];
              out[top + 1] = (uint8_t)work[work[i + 1] * 2];
            }
          }
          work[i] = top;
        }
      } else {
        uint16_t *out = (uint16_t *)s->dec_table;
        for (int32_t i = s->used_entries * 2 - 4; i >= 0; i -= 2) {
          if (work[i] & 0x80000000UL) {
            if (work[i + 1] & 0x80000000UL) {
              top -= 4;
              out[top] = (uint16_t)(work[i] >> 16 & 0x7fff) | 0x8000;
              out[top + 1] = (uint16_t)(work[i + 1] >> 16 & 0x7fff) | 0x8000;
              out[top + 2] = (uint16_t)work[i] & 0xffff;
              out[top + 3] = (uint16_t)work[i + 1] & 0xffff;
            } else {
              top -= 3;
              out[top] = (uint16_t)(work[i] >> 16 & 0x7fff) | 0x8000;
              out[top + 1] = (uint16_t)work[work[i + 1] * 2];
              out[top + 2] = (uint16_t)work[i] & 0xffff;
            }
          } else {
            if (work[i + 1] & 0x80000000UL) {
              top -= 3;
              out[top] = (uint16_t)work[work[i] * 2];
              out[top + 1] = (uint16_t)(work[i + 1] >> 16 & 0x7fff) | 0x8000;
              out[top + 2] = (uint16_t)work[i + 1] & 0xffff;
            } else {
              top -= 2;
              out[top] = (uint16_t)work[work[i] * 2];
              out[top + 1] = (uint16_t)work[work[i + 1] * 2];
            }
          }
          work[i] = (uint32_t)top;
        }
      }
    }
    if (work) {
      free(work);
      work = NULL;
    }
    return 0;
  }
  //-
  /* given a list of word lengths, number of used entries, and byte width of a
   * leaf, generate the decode table */
  int32_t make_words(char *l, uint16_t n, uint32_t *work, uint8_t quantvals,
                     codebook_t *b, int32_t maptype) {
    int32_t i, j, count = 0;
    uint32_t top = 0;
    uint32_t marker[33];

    if (n < 2) {
      work[0] = 0x80000000;
    } else {
      memset(marker, 0, sizeof(marker));

      for (i = 0; i < n; i++) {
        int32_t length = l[i];
        if (length) {
          uint32_t entry = marker[length];
          uint32_t chase = 0;
          if (count && !entry) return -1; /* overpopulated tree! */

          /* chase the tree as far as it's already populated, fill in past */
          for (j = 0; j < length - 1; j++) {
            uint32_t bit = (entry >> (length - j - 1)) & 1;
            if (chase >= top) {
              top++;
              work[chase * 2] = top;
              work[chase * 2 + 1] = 0;
            } else if (!work[chase * 2 + bit]) {
              work[chase * 2 + bit] = top;
            }
            chase = work[chase * 2 + bit];
          }
          {
            int32_t bit = (entry >> (length - j - 1)) & 1;
            if (chase >= top) {
              top++;
              work[chase * 2 + 1] = 0;
            }
            work[chase * 2 + bit] =
                decpack(i, count++, quantvals, b, maptype) | 0x80000000;
          }

          /* Look to see if the next shorter marker points to the node above. if
           * so, update it and repeat.  */
          for (j = length; j > 0; j) {
            if (marker[j] & 1) {
              marker[j] = marker[j - 1] << 1;
              break;
            }
            marker[j]++;
          }

          /* prune the tree; the implicit invariant says all the int32_ter
             markers were dangling from our just-taken node. Dangle them from
             our *new* node. */
          for (j = length + 1; j < 33; j++)
            if ((marker[j] >> 1) == entry) {
              entry = marker[j];
              marker[j] = marker[j - 1] << 1;
            } else
              break;
        }
      }
    }

    return 0;
  }
  //-
  uint32_t decpack(int32_t entry, int32_t used_entry, uint8_t quantvals,
                   codebook_t *b, int32_t maptype) {
    uint32_t ret = 0;

    switch (b->dec_type) {
      case 0:
        return (uint32_t)entry;

      case 1:
        if (maptype == 1) {
          /* vals are already read into temporary column vector here */
          assert(b->dim >= 0);
          for (uint8_t j = 0; j < b->dim; j++) {
            uint32_t off = (uint32_t)(entry % quantvals);
            entry /= quantvals;
            assert((b->q_bits * j) >= 0);
            uint32_t shift = (uint32_t)b->q_bits * j;
            ret |= ((uint16_t *)(b->q_val))[off] << shift;
          }
        } else {
          assert(b->dim >= 0);
          for (uint8_t j = 0; j < b->dim; j++) {
            assert((b->q_bits * j) >= 0);
            uint32_t shift = (uint32_t)b->q_bits * j;
            int32_t ret = bitReader(b->q_bits) << shift;
            assert(_ret >= 0);
            ret |= (uint32_t)_ret;
          }
        }
        return ret;

      case 2:
        assert(b->dim >= 0);
        for (uint8_t j = 0; j < b->dim; j++) {
          uint32_t off = uint32_t(entry % quantvals);
          entry /= quantvals;
          assert(b->q_pack * j >= 0);
          assert(b->q_pack * j <= 255);
          ret |= off << (uint8_t)(b->q_pack * j);
        }
        return ret;

      case 3:
        return (uint32_t)used_entry;
    }
    return 0; /* silence compiler */
  }
  //-
  /* most of the time, entries%dimensions == 0, but we need to be well defined.
   We define that the possible vales at each scalar is values == entries/dim. If
   entries%dim != 0, we'll have 'too few' values (values*dim<entries), which
   means that we'll have 'left over' entries; left over entries use zeroed
   values (and are wasted).  So don't generate codebooks like that */
  /* there might be a straightforward one-line way to do the below that's
   portable and totally safe against roundoff, but I haven't thought of it.
   Therefore, we opt on the side of caution */
  uint8_t book_maptype1_quantvals(codebook_t *b) {
    /* get us a starting hint, we'll polish it below */
    uint8_t bits = ilog(b->entries);
    uint8_t vals = b->entries >> ((bits - 1) * (b->dim - 1) / b->dim);

    while (1) {
      uint32_t acc = 1;
      uint32_t acc1 = 1;

      for (uint8_t i = 0; i < b->dim; i++) {
        acc *= vals;
        acc1 *= vals + 1;
      }
      if (acc <= b->entries && acc1 > b->entries) {
        return (vals);
      } else {
        if (acc > b->entries) {
          vals;
        } else {
          vals++;
        }
      }
    }
  }
  //-
  int32_t oggpack_eop() {
    if (s_bitReader.headptr - s_bitReader.data > s_setupHeaderLength) {
      LOGI("s_bitReader.headptr %i, s_setupHeaderLength %i",
           s_bitReader.headptr, s_setupHeaderLength);
      LOGI("ogg package 3 overflow");
      return -1;
    }
    return 0;
  }
  //-
  void book_clear(codebook_t *b) {
    /* static book is not cleared; we're likely called on the lookup and the
   static codebook beint32_ts to the info struct */
    if (b->q_val) free(b->q_val);
    if (b->dec_table) free(b->dec_table);

    memset(b, 0, sizeof(*b));
  }
  //-
  info_floor_t *floor0_info_unpack() {
    int32_t j;

    info_floor_t *info = (info_floor_t *)decoder_malloc(sizeof(*info));
    info->order = bitReader(8);
    info->rate = bitReader(16);
    info->barkmap = bitReader(16);
    info->ampbits = bitReader(6);
    info->ampdB = bitReader(8);
    info->numbooks = bitReader(4) + 1;

    if (info->order < 1) goto err_out;
    if (info->rate < 1) goto err_out;
    if (info->barkmap < 1) goto err_out;

    for (j = 0; j < info->numbooks; j++) {
      info->books[j] = bitReader(8);
      if (info->books[j] >= s_nrOfCodebooks) goto err_out;
    }

    if (oggpack_eop()) goto err_out;
    return (info);

  err_out:
    floor_free_info(info);
    return (NULL);
  }
  //-
  info_floor_t *floor1_info_unpack() {
    int32_t j, k, count = 0, maxclass = -1, rangebits;

    info_floor_t *info =
        (info_floor_t *)decoder_calloc(1, sizeof(info_floor_t));
    /* read partitions */
    info->partitions = bitReader(5); /* only 0 to 31 legal */
    info->partitionclass = (uint8_t *)decoder_malloc(
        info->partitions * sizeof(*info->partitionclass));
    for (j = 0; j < info->partitions; j++) {
      info->partitionclass[j] = bitReader(4); /* only 0 to 15 legal */
      if (maxclass < info->partitionclass[j])
        maxclass = info->partitionclass[j];
    }

    /* read partition classes */
    info->_class = (floor1class_t *)decoder_malloc(
        (uint32_t)(maxclass + 1) * sizeof(*info->_class));
    for (j = 0; j < maxclass + 1; j++) {
      info->_class[j].class_dim = bitReader(3) + 1; /* 1 to 8 */
      info->_class[j].class_subs = bitReader(2);    /* 0,1,2,3 bits */
      if (oggpack_eop() < 0) goto err_out;
      if (info->_class[j].class_subs) {
        info->_class[j].class_book = bitReader(8);
      } else {
        info->_class[j].class_book = 0;
      }
      if (info->_class[j].class_book >= s_nrOfCodebooks) goto err_out;
      for (k = 0; k < (1 << info->_class[j].class_subs); k++) {
        info->_class[j].class_subbook[k] = (uint8_t)bitReader(8) - 1;
        if (info->_class[j].class_subbook[k] >= s_nrOfCodebooks &&
            info->_class[j].class_subbook[k] != 0xff)
          goto err_out;
      }
    }

    /* read the post list */
    info->mult = bitReader(2) + 1; /* only 1,2,3,4 legal now */
    rangebits = bitReader(4);

    for (j = 0, k = 0; j < info->partitions; j++)
      count += info->_class[info->partitionclass[j]].class_dim;
    info->postlist =
        (uint16_t *)decoder_malloc((count + 2) * sizeof(*info->postlist));
    info->forward_index = (uint8_t *)decoder_malloc(
        (count + 2) * sizeof(*info->forward_index));
    info->loneighbor =
        (uint8_t *)decoder_malloc(count * sizeof(*info->loneighbor));
    info->hineighbor =
        (uint8_t *)decoder_malloc(count * sizeof(*info->hineighbor));

    count = 0;
    for (j = 0, k = 0; j < info->partitions; j++) {
      count += info->_class[info->partitionclass[j]].class_dim;
      if (count > VIF_POSIT) goto err_out;
      for (; k < count; k++) {
        int32_t t = info->postlist[k + 2] = bitReader(rangebits);
        if (t >= (1 << rangebits)) goto err_out;
      }
    }
    if (oggpack_eop()) goto err_out;
    info->postlist[0] = 0;
    info->postlist[1] = 1 << rangebits;
    info->posts = count + 2;

    /* also store a sorted position index */
    for (j = 0; j < info->posts; j++) info->forward_index[j] = j;
    mergesort(info->forward_index, info->postlist, info->posts);

    /* discover our neighbors for decode where we don't use fit flags (that
     * would push the neighbors outward) */
    for (j = 0; j < info->posts - 2; j++) {
      int32_t lo = 0;
      int32_t hi = 1;
      int32_t lx = 0;
      int32_t hx = info->postlist[1];
      int32_t currentx = info->postlist[j + 2];
      for (k = 0; k < j + 2; k++) {
        int32_t x = info->postlist[k];
        if (x > lx && x < currentx) {
          lo = k;
          lx = x;
        }
        if (x < hx && x > currentx) {
          hi = k;
          hx = x;
        }
      }
      info->loneighbor[j] = lo;
      info->hineighbor[j] = hi;
    }

    return (info);

  err_out:
    floor_free_info(info);
    return (NULL);
  }
  //-
  /* info is for range checking */
  int32_t res_unpack(info_residue_t *info) {
    int32_t j, k;
    memset(info, 0, sizeof(*info));

    info->type = bitReader(16);
    if (info->type > 2 || info->type < 0) goto errout;
    info->begin = bitReader(24);
    info->end = bitReader(24);
    info->grouping = bitReader(24) + 1;
    info->partitions = bitReader(6) + 1;
    info->groupbook = bitReader(8);
    if (info->groupbook >= s_nrOfCodebooks) goto errout;

    info->stagemasks = (uint8_t *)decoder_malloc(
        info->partitions * sizeof(*info->stagemasks));
    info->stagebooks = (uint8_t *)decoder_malloc(
        info->partitions * 8 * sizeof(*info->stagebooks));

    for (j = 0; j < info->partitions; j++) {
      int32_t cascade = bitReader(3);
      if (bitReader(1)) cascade |= (bitReader(5) << 3);
      info->stagemasks[j] = cascade;
    }

    for (j = 0; j < info->partitions; j++) {
      for (k = 0; k < 8; k++) {
        if ((info->stagemasks[j] >> k) & 1) {
          uint8_t book = bitReader(8);
          if (book >= s_nrOfCodebooks) goto errout;
          info->stagebooks[j * 8 + k] = book;
          if (k + 1 > info->stages) info->stages = k + 1;
        } else
          info->stagebooks[j * 8 + k] = 0xff;
      }
    }

    if (oggpack_eop()) goto errout;

    return 0;
  errout:
    res_clear_info(info);
    return 1;
  }
  //-
  /* also responsible for range checking */
  int32_t mapping_info_unpack(info_mapping_t *info) {
    int32_t i;
    memset(info, 0, sizeof(*info));

    if (bitReader(1))
      info->submaps = bitReader(4) + 1;
    else
      info->submaps = 1;

    if (bitReader(1)) {
      info->coupling_steps = bitReader(8) + 1;
      info->coupling = (coupling_step_t *)decoder_malloc(
          info->coupling_steps * sizeof(*info->coupling));

      for (i = 0; i < info->coupling_steps; i++) {
        int32_t testM = info->coupling[i].mag =
            bitReader(ilog(s_vorbisChannels));
        int32_t testA = info->coupling[i].ang =
            bitReader(ilog(s_vorbisChannels));

        if (testM < 0 || testA < 0 || testM == testA ||
            testM >= s_vorbisChannels || testA >= s_vorbisChannels)
          goto err_out;
      }
    }

    if (bitReader(2) > 0) goto err_out;
    /* 2,3:reserved */

    if (info->submaps > 1) {
      info->chmuxlist = (uint8_t *)decoder_malloc(
          sizeof(*info->chmuxlist) * s_vorbisChannels);
      for (i = 0; i < s_vorbisChannels; i++) {
        info->chmuxlist[i] = bitReader(4);
        if (info->chmuxlist[i] >= info->submaps) goto err_out;
      }
    }

    info->submaplist = (submap_t *)decoder_malloc(
        sizeof(*info->submaplist) * info->submaps);
    for (i = 0; i < info->submaps; i++) {
      int32_t temp = bitReader(8);
      (void)temp;
      info->submaplist[i].floor = bitReader(8);
      if (info->submaplist[i].floor >= s_nrOfFloors) goto err_out;
      info->submaplist[i].residue = bitReader(8);
      if (info->submaplist[i].residue >= s_nrOfResidues) goto err_out;
    }

    return 0;

  err_out:
    mapping_clear_info(info);
    return -1;
  }
  //-
  void mergesort(uint8_t *index, uint16_t *vals, uint16_t n) {
    uint16_t i, j;
    uint8_t *temp;
    uint8_t *A = index;
    uint8_t *B = (uint8_t *)decoder_malloc(n * sizeof(*B));

    for (i = 1; i < n; i <<= 1) {
      for (j = 0; j + i < n;) {
        uint16_t k1 = j;
        uint16_t mid = j + i;
        uint16_t k2 = mid;
        int32_t end = (j + i * 2 < n ? j + i * 2 : n);
        while (k1 < mid && k2 < end) {
          if (vals[A[k1]] < vals[A[k2]])
            B[j++] = A[k1++];
          else
            B[j++] = A[k2++];
        }
        while (k1 < mid) B[j++] = A[k1++];
        while (k2 < end) B[j++] = A[k2++];
      }
      for (; j < n; j++) B[j] = A[j];
      temp = A;
      A = B;
      B = temp;
    }

    if (B == index) {
      for (j = 0; j < n; j++) B[j] = A[j];
      free(A);
    } else
      free(B);
  }
  //-
  void floor_free_info(info_floor_t *i) {
    info_floor_t *info = (info_floor_t *)i;
    if (info) {
      if (info->_class) {
        free(info->_class);
      }
      if (info->partitionclass) {
        free(info->partitionclass);
      }
      if (info->postlist) {
        free(info->postlist);
      }
      if (info->forward_index) {
        free(info->forward_index);
      }
      if (info->hineighbor) {
        free(info->hineighbor);
      }
      if (info->loneighbor) {
        free(info->loneighbor);
      }
      memset(info, 0, sizeof(*info));
      if (info) free(info);
    }
  }
  //-
  void res_clear_info(info_residue_t *info) {
    if (info) {
      if (info->stagemasks) free(info->stagemasks);
      if (info->stagebooks) free(info->stagebooks);
      memset(info, 0, sizeof(*info));
    }
  }
  //-
  void mapping_clear_info(info_mapping_t *info) {
    if (info) {
      if (info->chmuxlist) free(info->chmuxlist);
      if (info->submaplist) free(info->submaplist);
      if (info->coupling) free(info->coupling);
      memset(info, 0, sizeof(*info));
    }
  }
  //-
  //      ⏫⏫⏫    O G G      I M P L     A B O V E  ⏫⏫⏫
  //      ⏬⏬⏬ V O R B I S   I M P L     B E L O W  ⏬⏬⏬
  //-
  dsp_state_t *dsp_create() {
    int32_t i;

    dsp_state_t *v = (dsp_state_t *)decoder_calloc(1, sizeof(dsp_state_t));

    v->work =
        (int32_t **)decoder_malloc(s_vorbisChannels * sizeof(*v->work));
    v->mdctright = (int32_t **)decoder_malloc(s_vorbisChannels *
                                                   sizeof(*v->mdctright));

    for (i = 0; i < s_vorbisChannels; i++) {
      v->work[i] = (int32_t *)decoder_calloc(
          1, (s_blocksizes[1] >> 1) * sizeof(*v->work[i]));
      v->mdctright[i] = (int32_t *)decoder_calloc(
          1, (s_blocksizes[1] >> 2) * sizeof(*v->mdctright[i]));
    }

    v->lW = 0; /* previous window size */
    v->W = 0;  /* current window size  */

    v->out_end = -1;  // dsp_restart
    v->out_begin = -1;

    return v;
  }
  //-
  void dsp_destroy(dsp_state_t *v) {
    int32_t i;
    if (v) {
      if (v->work) {
        for (i = 0; i < s_vorbisChannels; i++) {
          if (v->work[i]) {
            free(v->work[i]);
            v->work[i] = NULL;
          }
        }
        if (v->work) {
          free(v->work);
          v->work = NULL;
        }
      }
      if (v->mdctright) {
        for (i = 0; i < s_vorbisChannels; i++) {
          if (v->mdctright[i]) {
            free(v->mdctright[i]);
            v->mdctright[i] = NULL;
          }
        }
        if (v->mdctright) {
          free(v->mdctright);
          v->mdctright = NULL;
        }
      }
      free(v);
      v = NULL;
    }
  }
  //-
  int32_t dsp_synthesis(uint8_t *inbuf, uint16_t len, int16_t *outbuf) {
    int32_t mode, i;

    /* Check the packet type */
    if (bitReader(1) != 0) {
      /* Oops.  This is not an audio data packet */
      return OV_ENOTAUDIO;
    }

    /* read our mode and pre/post windowsize */
    mode = bitReader(ilog(s_nrOfModes));
    if (mode == -1 || mode >= s_nrOfModes) return OV_EBADPACKET;

    /* shift information we still need from last window */
    s_dsp_state->lW = s_dsp_state->W;
    s_dsp_state->W = s_mode_param[mode].blockflag;
    for (i = 0; i < s_vorbisChannels; i++) {
      mdct_shift_right(s_blocksizes[s_dsp_state->lW], s_dsp_state->work[i],
                       s_dsp_state->mdctright[i]);
    }
    if (s_dsp_state->W) {
      int32_t temp;
      bitReader(1);
      temp = bitReader(1);
      if (temp == -1) return OV_EBADPACKET;
    }

    /* packet decode and portions of synthesis that rely on only this block */
    {
      mapping_inverse(s_map_param + s_mode_param[mode].mapping);

      if (s_dsp_state->out_begin == -1) {
        s_dsp_state->out_begin = 0;
        s_dsp_state->out_end = 0;
      } else {
        s_dsp_state->out_begin = 0;
        s_dsp_state->out_end = s_blocksizes[s_dsp_state->lW] / 4 +
                               s_blocksizes[s_dsp_state->W] / 4;
      }
    }

    return (0);
  }
  //-
  void mdct_shift_right(int32_t n, int32_t *in, int32_t *right) {
    int32_t i;
    n >>= 2;
    in += 1;

    for (i = 0; i < n; i++) right[i] = in[i << 1];
  }
  //-
  int32_t mapping_inverse(info_mapping_t *info) {
    int32_t i, j;
    int32_t n = s_blocksizes[s_dsp_state->W];

    int32_t **pcmbundle =
        (int32_t **)alloca(sizeof(*pcmbundle) * s_vorbisChannels);
    int32_t *zerobundle =
        (int32_t *)alloca(sizeof(*zerobundle) * s_vorbisChannels);
    int32_t *nonzero = (int32_t *)alloca(sizeof(*nonzero) * s_vorbisChannels);
    int32_t **floormemo =
        (int32_t **)alloca(sizeof(*floormemo) * s_vorbisChannels);

    /* recover the spectral envelope; store it in the PCM vector for now */
    for (i = 0; i < s_vorbisChannels; i++) {
      int32_t submap = 0;
      int32_t floorno;

      if (info->submaps > 1) submap = info->chmuxlist[i];
      floorno = info->submaplist[submap].floor;

      if (s_floor_type[floorno]) {
        /* floor 1 */
        floormemo[i] = (int32_t *)alloca(
            sizeof(*floormemo[i]) * floor1_memosize(s_floor_param[floorno]));
        floormemo[i] = floor1_inverse1(s_floor_param[floorno], floormemo[i]);
      } else {
        /* floor 0 */
        floormemo[i] = (int32_t *)alloca(
            sizeof(*floormemo[i]) * floor0_memosize(s_floor_param[floorno]));
        floormemo[i] = floor0_inverse1(s_floor_param[floorno], floormemo[i]);
      }

      if (floormemo[i])
        nonzero[i] = 1;
      else
        nonzero[i] = 0;
      memset(s_dsp_state->work[i], 0, sizeof(*s_dsp_state->work[i]) * n / 2);
    }

    /* channel coupling can 'dirty' the nonzero listing */
    for (i = 0; i < info->coupling_steps; i++) {
      if (nonzero[info->coupling[i].mag] || nonzero[info->coupling[i].ang]) {
        nonzero[info->coupling[i].mag] = 1;
        nonzero[info->coupling[i].ang] = 1;
      }
    }

    /* recover the residue into our working vectors */
    for (i = 0; i < info->submaps; i++) {
      uint8_t ch_in_bundle = 0;
      for (j = 0; j < s_vorbisChannels; j++) {
        if (!info->chmuxlist || info->chmuxlist[j] == i) {
          if (nonzero[j])
            zerobundle[ch_in_bundle] = 1;
          else
            zerobundle[ch_in_bundle] = 0;
          pcmbundle[ch_in_bundle++] = s_dsp_state->work[j];
        }
      }

      res_inverse(s_residue_param + info->submaplist[i].residue, pcmbundle,
                  zerobundle, ch_in_bundle);
    }

    // for(j=0;j<vi->channels;j++)
    //_analysis_output("coupled",seq+j,vb->pcm[j],-8,n/2,0,0);

    /* channel coupling */
    for (i = info->coupling_steps - 1; i >= 0; i) {
      int32_t *pcmM = s_dsp_state->work[info->coupling[i].mag];
      int32_t *pcmA = s_dsp_state->work[info->coupling[i].ang];

      for (j = 0; j < n / 2; j++) {
        int32_t mag = pcmM[j];
        int32_t ang = pcmA[j];

        if (mag > 0)
          if (ang > 0) {
            pcmM[j] = mag;
            pcmA[j] = mag - ang;
          } else {
            pcmA[j] = mag;
            pcmM[j] = mag + ang;
          }
        else if (ang > 0) {
          pcmM[j] = mag;
          pcmA[j] = mag + ang;
        } else {
          pcmA[j] = mag;
          pcmM[j] = mag - ang;
        }
      }
    }

    // for(j=0;j<vi->channels;j++)
    //_analysis_output("residue",seq+j,vb->pcm[j],-8,n/2,0,0);

    /* compute and apply spectral envelope */

    for (i = 0; i < s_vorbisChannels; i++) {
      int32_t *pcm = s_dsp_state->work[i];
      int32_t submap = 0;
      int32_t floorno;

      if (info->submaps > 1) submap = info->chmuxlist[i];
      floorno = info->submaplist[submap].floor;

      if (s_floor_type[floorno]) {
        /* floor 1 */
        floor1_inverse2(s_floor_param[floorno], floormemo[i], pcm);
      } else {
        /* floor 0 */
        floor0_inverse2(s_floor_param[floorno], floormemo[i], pcm);
      }
    }

    // for(j=0;j<vi->channels;j++)
    //_analysis_output("mdct",seq+j,vb->pcm[j],-24,n/2,0,1);

    /* transform the PCM data; takes PCM vector, vb; modifies PCM vector */
    /* only MDCT right now.... */
    for (i = 0; i < s_vorbisChannels; i++) {
      mdct_backward(n, s_dsp_state->work[i]);
    }

    // for(j=0;j<vi->channels;j++)
    //_analysis_output("imdct",seq+j,vb->pcm[j],-24,n,0,0);

    /* all done! */
    return (0);
  }
  //-
  int32_t floor0_memosize(info_floor_t *i) {
    info_floor_t *info = (info_floor_t *)i;
    return info->order + 1;
  }
  //-
  int32_t floor1_memosize(info_floor_t *i) {
    info_floor_t *info = (info_floor_t *)i;
    return info->posts;
  }
  //-
  int32_t *floor0_inverse1(info_floor_t *i, int32_t *lsp) {
    info_floor_t *info = (info_floor_t *)i;
    int32_t j;

    int32_t ampraw = bitReader(info->ampbits);

    if (ampraw > 0) { /* also handles the -1 out of data case */
      int32_t maxval = (1 << info->ampbits) - 1;
      int32_t amp = ((ampraw * info->ampdB) << 4) / maxval;
      int32_t booknum = bitReader(_ilog(info->numbooks));

      if (booknum != -1 && booknum < info->numbooks) { /* be paranoid */
        codebook_t *b = s_codebooks + info->books[booknum];
        int32_t last = 0;

        if (book_decodev_set(b, lsp, info->order, -24) == -1) goto eop;
        for (j = 0; j < info->order;) {
          for (uint8_t k = 0; j < info->order && k < b->dim; k++, j++)
            lsp[j] += last;
          last = lsp[j - 1];
        }

        lsp[info->order] = amp;
        return (lsp);
      }
    }
  eop:
    return (NULL);
  }
  //-
  int32_t *floor1_inverse1(info_floor_t *in, int32_t *fit_value) {
    info_floor_t *info = (info_floor_t *)in;

    int32_t quant_look[4] = {256, 128, 86, 64};
    int32_t i, j, k;
    int32_t quant_q = quant_look[info->mult - 1];
    codebook_t *books = s_codebooks;

    /* unpack wrapped/predicted values from stream */
    if (bitReader(1) == 1) {
      fit_value[0] = bitReader(ilog(quant_q - 1));
      fit_value[1] = bitReader(ilog(quant_q - 1));

      /* partition by partition */
      for (i = 0, j = 2; i < info->partitions; i++) {
        int32_t classv = info->partitionclass[i];
        int32_t cdim = info->_class[classv].class_dim;
        int32_t csubbits = info->_class[classv].class_subs;
        int32_t csub = 1 << csubbits;
        int32_t cval = 0;

        /* decode the partition's first stage cascade value */
        if (csubbits) {
          cval = book_decode(books + info->_class[classv].class_book);
          if (cval == -1) goto eop;
        }

        for (k = 0; k < cdim; k++) {
          int32_t book = info->_class[classv].class_subbook[cval & (csub - 1)];
          cval >>= csubbits;
          if (book != 0xff) {
            if ((fit_value[j + k] = book_decode(books + book)) == -1) goto eop;
          } else {
            fit_value[j + k] = 0;
          }
        }
        j += cdim;
      }

      /* unwrap positive values and reconsitute via linear interpolation */
      for (i = 2; i < info->posts; i++) {
        int32_t predicted =
            render_point(info->postlist[info->loneighbor[i - 2]],
                         info->postlist[info->hineighbor[i - 2]],
                         fit_value[info->loneighbor[i - 2]],
                         fit_value[info->hineighbor[i - 2]], info->postlist[i]);
        int32_t hiroom = quant_q - predicted;
        int32_t loroom = predicted;
        int32_t room = (hiroom < loroom ? hiroom : loroom) << 1;
        int32_t val = fit_value[i];

        if (val) {
          if (val >= room) {
            if (hiroom > loroom) {
              val = val - loroom;
            } else {
              val = -1 - (val - hiroom);
            }
          } else {
            if (val & 1) {
              val = -((val + 1) >> 1);
            } else {
              val >>= 1;
            }
          }

          fit_value[i] = val + predicted;
          fit_value[info->loneighbor[i - 2]] &= 0x7fff;
          fit_value[info->hineighbor[i - 2]] &= 0x7fff;
        } else {
          fit_value[i] = predicted | 0x8000;
        }
      }

      return (fit_value);
    } else {
      // LOGI("err in br");
      ;
    }
  eop:
    return (NULL);
  }
  //-
  /* returns the [original, not compacted] entry number or -1 on eof *********/
  int32_t book_decode(codebook_t *book) {
    if (book->dec_type) return -1;
    return decode_packed_entry_number(book);
  }
  //-
  int32_t decode_packed_entry_number(codebook_t *book) {
    uint32_t chase = 0;
    int32_t read = book->dec_maxlength;
    int32_t lok = bitReader_look(read), i;

    while (lok < 0 && read > 1) {
      lok = bitReader_look(read);
    }

    if (lok < 0) {
      bitReader_adv(1); /* force eop */
      return -1;
    }

    /* chase the tree with the bits we got */
    if (book->dec_nodeb == 1) {
      if (book->dec_leafw == 1) {
        /* 8/8 */
        uint8_t *t = (uint8_t *)book->dec_table;
        for (i = 0; i < read; i++) {
          chase = t[chase * 2 + ((lok >> i) & 1)];
          if (chase & 0x80UL) break;
        }
        chase &= 0x7fUL;
      } else {
        /* 8/16 */
        uint8_t *t = (uint8_t *)book->dec_table;
        for (i = 0; i < read; i++) {
          int32_t bit = (lok >> i) & 1;
          int32_t next = t[chase + bit];
          if (next & 0x80) {
            chase =
                (next << 8) | t[chase + bit + 1 + (!bit || (t[chase] & 0x80))];
            break;
          }
          chase = next;
        }
        chase &= 0x7fffUL;
      }
    } else {
      if (book->dec_nodeb == 2) {
        if (book->dec_leafw == 1) {
          /* 16/16 */
          int32_t idx;
          for (i = 0; i < read; i++) {
            idx = chase * 2 + ((lok >> i) & 1);
            chase = ((uint16_t *)(book->dec_table))[idx];
            if (chase & 0x8000UL) {
              break;
            }
          }
          chase &= 0x7fffUL;
        } else {
          /* 16/32 */
          uint16_t *t = (uint16_t *)book->dec_table;
          for (i = 0; i < read; i++) {
            int32_t bit = (lok >> i) & 1;
            int32_t next = t[chase + bit];
            if (next & 0x8000) {
              chase = (next << 16) |
                      t[chase + bit + 1 + (!bit || (t[chase] & 0x8000))];
              break;
            }
            chase = next;
          }
          chase &= 0x7fffffffUL;
        }
      } else {
        for (i = 0; i < read; i++) {
          chase = ((uint32_t *)(book->dec_table))[chase * 2 + ((lok >> i) & 1)];
          if (chase & 0x80000000UL) break;
        }
        chase &= 0x7fffffffUL;
      }
    }

    if (i < read) {
      bitReader_adv(i + 1);
      return chase;
    }
    bitReader_adv(read + 1);
    LOGE("read %i", read);
    return (-1);
  }
  //-
  int32_t render_point(int32_t x0, int32_t x1, int32_t y0, int32_t y1,
                       int32_t x) {
    y0 &= 0x7fff; /* mask off flag */
    y1 &= 0x7fff;

    {
      int32_t dy = y1 - y0;
      int32_t adx = x1 - x0;
      int32_t ady = abs(dy);
      int32_t err = ady * (x - x0);

      int32_t off = err / adx;
      if (dy < 0) return (y0 - off);
      return (y0 + off);
    }
  }
  //-
  /* unlike the others, we guard against n not being an integer number * of
   <dim> internally rather than in the upper layer (called only by * floor0) */
  int32_t book_decodev_set(codebook_t *book, int32_t *a, int32_t n,
                           int32_t point) {
    if (book->used_entries > 0) {
      int32_t *v = (int32_t *)alloca(sizeof(*v) * book->dim);
      int32_t i;

      for (i = 0; i < n;) {
        if (decode_map(book, v, point)) return -1;
        for (uint8_t j = 0; i < n && j < book->dim; j++) a[i++] = v[j];
      }
    } else {
      int32_t i;

      for (i = 0; i < n;) {
        a[i++] = 0;
      }
    }

    return 0;
  }
  //-
  int32_t decode_map(codebook_t *s, int32_t *v, int32_t point) {
    uint32_t entry = decode_packed_entry_number(s);

    if (oggpack_eop()) return (-1);

    /* according to decode type */
    switch (s->dec_type) {
      case 1: {
        /* packed vector of values */
        int32_t mask = (1 << s->q_bits) - 1;
        for (uint8_t i = 0; i < s->dim; i++) {
          v[i] = entry & mask;
          entry >>= s->q_bits;
        }
        break;
      }
      case 2: {
        /* packed vector of column offsets */
        int32_t mask = (1 << s->q_pack) - 1;
        for (uint8_t i = 0; i < s->dim; i++) {
          if (s->q_bits <= 8)
            v[i] = ((uint8_t *)(s->q_val))[entry & mask];
          else
            v[i] = ((uint16_t *)(s->q_val))[entry & mask];
          entry >>= s->q_pack;
        }
        break;
      }
      case 3: {
        /* offset into array */
        void *ptr = (int32_t *)s->q_val + entry * s->q_pack;

        if (s->q_bits <= 8) {
          for (uint8_t i = 0; i < s->dim; i++) v[i] = ((uint8_t *)ptr)[i];
        } else {
          for (uint8_t i = 0; i < s->dim; i++) v[i] = ((uint16_t *)ptr)[i];
        }
        break;
      }
      default:
        return -1;
    }

    /* we have the unpacked multiplicands; compute final vals */
    {
      int32_t shiftM = point - s->q_delp;
      int32_t add = point - s->q_minp;
      if (add > 0)
        add = s->q_min >> add;
      else
        add = s->q_min << -add;

      if (shiftM > 0)
        for (uint8_t i = 0; i < s->dim; i++)
          v[i] = add + ((v[i] * s->q_del) >> shiftM);
      else
        for (uint8_t i = 0; i < s->dim; i++)
          v[i] = add + ((v[i] * s->q_del) << -shiftM);

      if (s->q_seq)
        for (uint8_t i = 1; i < s->dim; i++) v[i] += v[i - 1];
    }

    return 0;
  }
  //-
  int32_t res_inverse(info_residue_t *info, int32_t **in, int32_t *nonzero,
                      uint8_t ch) {
    int32_t j, k, s;
    uint8_t m = 0, n = 0;
    uint8_t used = 0;
    codebook_t *phrasebook = s_codebooks + info->groupbook;
    uint32_t samples_per_partition = info->grouping;
    uint8_t partitions_per_word = phrasebook->dim;
    uint32_t pcmend = s_blocksizes[s_dsp_state->W];

    if (info->type < 2) {
      uint32_t max = pcmend >> 1;
      uint32_t end = (info->end < max ? info->end : max);
      uint32_t n1 = end - info->begin;

      if (n1 > 0) {
        uint32_t partvals = n1 / samples_per_partition;
        uint32_t partwords =
            (partvals + partitions_per_word - 1) / partitions_per_word;

        for (uint8_t i = 0; i < ch; i++) {
          if (nonzero[i]) in[used++] = in[i];
        }
        ch = used;

        if (used) {
          char **partword = (char **)alloca(ch * sizeof(*partword));
          for (j = 0; j < ch; j++) {
            partword[j] = (char *)alloca(partwords * partitions_per_word *
                                         sizeof(*partword[j]));
          }
          for (s = 0; s < info->stages; s++) {
            for (uint32_t i = 0; i < partvals;) {
              if (s == 0) {
                /* fetch the partition word for each channel */
                partword[0][i + partitions_per_word - 1] = 1;
                for (k = partitions_per_word - 2; k >= 0; k) {
                  partword[0][i + k] =
                      partword[0][i + k + 1] * info->partitions;
                }
                for (j = 1; j < ch; j++) {
                  for (k = partitions_per_word - 1; k >= 0; k) {
                    partword[j][i + k] = partword[j - 1][i + k];
                  }
                }
                for (n = 0; n < ch; n++) {
                  int32_t temp = book_decode(phrasebook);
                  if (temp == -1) goto eopbreak;
                  /* this can be done quickly in assembly due to the quotient
                   always being at most six bits */
                  for (m = 0; m < partitions_per_word; m++) {
                    char div = partword[n][i + m];
                    partword[n][i + m] = temp / div;
                    temp -= partword[n][i + m] * div;
                  }
                }
              }

              /* now we decode residual values for the partitions */
              for (k = 0; k < partitions_per_word && i < partvals; k++, i++) {
                for (j = 0; j < ch; j++) {
                  uint32_t offset = info->begin + i * samples_per_partition;
                  if (info->stagemasks[(int32_t)partword[j][i]] & (1 << s)) {
                    codebook_t *stagebook =
                        s_codebooks +
                        info->stagebooks[(partword[j][i] << 3) + s];
                    if (info->type) {
                      if (book_decodev_add(stagebook, in[j] + offset,
                                           samples_per_partition, -8) == -1)
                        goto eopbreak;
                    } else {
                      if (book_decodevs_add(stagebook, in[j] + offset,
                                            samples_per_partition, -8) == -1)
                        goto eopbreak;
                    }
                  }
                }
              }
            }
          }
        }
      }
    } else {
      uint32_t max = (pcmend * ch) >> 1;
      uint32_t end = (info->end < max ? info->end : max);
      uint32_t n = end - info->begin;

      if (n > 0) {
        uint32_t partvals = n / samples_per_partition;
        uint32_t partwords =
            (partvals + partitions_per_word - 1) / partitions_per_word;

        char *partword =
            (char *)alloca(partwords * partitions_per_word * sizeof(*partword));
        int32_t beginoff = info->begin / ch;

        uint8_t i = 0;
        for (i = 0; i < ch; i++)
          if (nonzero[i]) break;
        if (i == ch) return (0); /* no nonzero vectors */

        samples_per_partition /= ch;

        for (s = 0; s < info->stages; s++) {
          for (uint32_t i = 0; i < partvals;) {
            if (s == 0) {
              int32_t temp;
              partword[i + partitions_per_word - 1] = 1;
              for (k = partitions_per_word - 2; k >= 0; k)
                partword[i + k] = partword[i + k + 1] * info->partitions;

              /* fetch the partition word */
              temp = book_decode(phrasebook);
              if (temp == -1) goto eopbreak;

              /* this can be done quickly in assembly due to the quotient always
               * being at most six bits */
              for (k = 0; k < partitions_per_word; k++) {
                char div = partword[i + k];
                partword[i + k] = (char)temp / div;
                temp -= partword[i + k] * div;
              }
            }

            /* now we decode residual values for the partitions */
            for (k = 0; k < partitions_per_word && i < partvals; k++, i++)
              if (info->stagemasks[(int32_t)partword[i]] & (1 << s)) {
                codebook_t *stagebook =
                    s_codebooks + info->stagebooks[(partword[i] << 3) + s];
                if (book_decodevv_add(stagebook, in,
                                      i * samples_per_partition + beginoff, ch,
                                      samples_per_partition, -8) == -1)
                  goto eopbreak;
              }
          }
        }
      }
    }
  eopbreak:

    return 0;
  }
  //-
  /* decode vector / dim granularity guarding is done in the upper layer */
  int32_t book_decodev_add(codebook_t *book, int32_t *a, int32_t n,
                           int32_t point) {
    if (book->used_entries > 0) {
      int32_t *v = (int32_t *)alloca(sizeof(*v) * book->dim);
      uint32_t i;

      for (i = 0; i < n;) {
        if (decode_map(book, v, point)) return -1;
        for (uint8_t j = 0; i < n && j < book->dim; j++) a[i++] += v[j];
      }
    }
    return 0;
  }
  //-
  /* returns 0 on OK or -1 on eof */
  /* decode vector / dim granularity guarding is done in the upper layer */
  int32_t book_decodevs_add(codebook_t *book, int32_t *a, int32_t n,
                            int32_t point) {
    if (book->used_entries > 0) {
      int32_t step = n / book->dim;
      int32_t *v = (int32_t *)alloca(sizeof(*v) * book->dim);
      int32_t j;

      for (j = 0; j < step; j++) {
        if (decode_map(book, v, point)) return -1;
        for (uint8_t i = 0, o = j; i < book->dim; i++, o += step) a[o] += v[i];
      }
    }
    return 0;
  }
  //-
  int32_t floor0_inverse2(info_floor_t *i, int32_t *lsp, int32_t *out) {
    info_floor_t *info = (info_floor_t *)i;

    if (lsp) {
      int32_t amp = lsp[info->order];

      /* take the coefficients back to a spectral envelope curve */
      lsp_to_curve(out, s_blocksizes[s_dsp_state->W] / 2, info->barkmap, lsp,
                   info->order, amp, info->ampdB, info->rate >> 1);
      return (1);
    }
    memset(out, 0, sizeof(*out) * s_blocksizes[s_dsp_state->W] / 2);
    return (0);
  }

  //-
  int32_t floor1_inverse2(info_floor_t *in, int32_t *fit_value, int32_t *out) {
    info_floor_t *info = (info_floor_t *)in;

    int32_t n = s_blocksizes[s_dsp_state->W] / 2;
    int32_t j;

    if (fit_value) {
      /* render the lines */
      int32_t hx = 0;
      int32_t lx = 0;
      int32_t ly = fit_value[0] * info->mult;

      for (j = 1; j < info->posts; j++) {
        int32_t current = info->forward_index[j];
        int32_t hy = fit_value[current] & 0x7fff;
        if (hy == fit_value[current]) {
          hy *= info->mult;
          hx = info->postlist[current];

          render_line(n, lx, hx, ly, hy, out);

          lx = hx;
          ly = hy;
        }
      }
      for (j = hx; j < n; j++) out[j] *= ly; /* be certain */
      return (1);
    }
    memset(out, 0, sizeof(*out) * n);
    return (0);
  }
  //-
  void render_line(int32_t n, int32_t x0, int32_t x1, int32_t y0, int32_t y1,
                   int32_t *d) {
    int32_t dy = y1 - y0;
    int32_t adx = x1 - x0;
    int32_t ady = abs(dy);
    int32_t base = dy / adx;
    int32_t sy = (dy < 0 ? base - 1 : base + 1);
    int32_t x = x0;
    int32_t y = y0;
    int32_t err = 0;

    if (n > x1) n = x1;
    ady -= abs(base * adx);

    if (x < n) {
      d[x] = MULT31_SHIFT15(d[x], FLOOR_fromdB_LOOKUP[y]);
    }

    while (++x < n) {
      err = err + ady;
      if (err >= adx) {
        err -= adx;
        y += sy;
      } else {
        y += base;
      }
      d[x] = MULT31_SHIFT15(d[x], FLOOR_fromdB_LOOKUP[y]);
    }
  }
  //-
  const uint16_t barklook[54] = {
      0,    51,    102,   154,   206,   258,   311,   365,   420,   477,  535,
      594,  656,   719,   785,   854,   926,   1002,  1082,  1166,  1256, 1352,
      1454, 1564,  1683,  1812,  1953,  2107,  2276,  2463,  2670,  2900, 3155,
      3440, 3756,  4106,  4493,  4919,  5387,  5901,  6466,  7094,  7798, 8599,
      9528, 10623, 11935, 13524, 15453, 17775, 20517, 23667, 27183, 31004};

  const uint8_t MLOOP_1[64] = {
      0,  10, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
      15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
  };

  const uint8_t MLOOP_2[64] = {
      0, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8,
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
      9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  };

  const uint8_t MLOOP_3[8] = {0, 1, 2, 2, 3, 3, 3, 3};

  /* interpolated 1./sqrt(p) where .5 <= a < 1. (.100000... to .111111...)
   * in 16.16 format returns in m.8 format */
  int32_t ADJUST_SQRT2[2] = {8192, 5792};

  //-
  void lsp_to_curve(int32_t *curve, int32_t n, int32_t ln, int32_t *lsp,
                    int32_t m, int32_t amp, int32_t ampoffset, int32_t nyq) {
    /* 0 <= m < 256 */

    /* set up for using all int32_t later */
    int32_t i;
    int32_t ampoffseti = ampoffset * 4096;
    int32_t ampi = amp;
    int32_t *ilsp = (int32_t *)alloca(m * sizeof(*ilsp));
    uint32_t imap = (1UL << 31) / ln;
    uint32_t tBnyq1 = toBARK(nyq) << 1;

    /* Besenham for frequency scale to avoid a division */
    int32_t f = 0;
    int32_t fdx = n;
    int32_t fbase = nyq / fdx;
    int32_t ferr = 0;
    int32_t fdy = nyq - fbase * fdx;
    int32_t map = 0;

    uint32_t nextbark = MULT31(imap >> 1, tBnyq1);

    int32_t nextf = barklook[nextbark >> 14] +
                    (((nextbark & 0x3fff) * (barklook[(nextbark >> 14) + 1] -
                                             barklook[nextbark >> 14])) >>
                     14);

    /* lsp is in 8.24, range 0 to PI; coslook wants it in .16 0 to 1*/
    for (i = 0; i < m; i++) {
      int32_t val = MULT32(lsp[i], 0x517cc2);
      /* safeguard against a malicious stream */
      if (val < 0 || (val >> COS_LOOKUP_I_SHIFT) >= COS_LOOKUP_I_SZ) {
        memset(curve, 0, sizeof(*curve) * n);
        return;
      }

      ilsp[i] = coslook_i(val);
    }

    i = 0;
    while (i < n) {
      int32_t j;
      uint32_t pi = 46341; /* 2**-.5 in 0.16 */
      uint32_t qi = 46341;
      int32_t qexp = 0, shift;
      int32_t wi;

      wi = coslook2_i((map * imap) >> 15);

      qi *= labs(ilsp[0] - wi);
      pi *= labs(ilsp[1] - wi);

      for (j = 3; j < m; j += 2) {
        if (!(shift = MLOOP_1[(pi | qi) >> 25]))
          if (!(shift = MLOOP_2[(pi | qi) >> 19]))
            shift = MLOOP_3[(pi | qi) >> 16];

        qi = (qi >> shift) * labs(ilsp[j - 1] - wi);
        pi = (pi >> shift) * labs(ilsp[j] - wi);
        qexp += shift;
      }
      if (!(shift = MLOOP_1[(pi | qi) >> 25]))
        if (!(shift = MLOOP_2[(pi | qi) >> 19]))
          shift = MLOOP_3[(pi | qi) >> 16];

      /* pi,qi normalized collectively, both tracked using qexp */

      if (m & 1) {
        /* odd order filter; slightly assymetric */
        /* the last coefficient */
        qi = (qi >> shift) * labs(ilsp[j - 1] - wi);
        pi = (pi >> shift) << 14;
        qexp += shift;

        if (!(shift = MLOOP_1[(pi | qi) >> 25]))
          if (!(shift = MLOOP_2[(pi | qi) >> 19]))
            shift = MLOOP_3[(pi | qi) >> 16];

        pi >>= shift;
        qi >>= shift;
        qexp += shift - 14 * ((m + 1) >> 1);

        pi = ((pi * pi) >> 16);
        qi = ((qi * qi) >> 16);
        qexp = qexp * 2 + m;

        pi *= (1 << 14) - ((wi * wi) >> 14);
        qi += pi >> 14;
      } else {
        /* even order filter; still symmetric */
        /* p*=p(1-w), q*=q(1+w), let normalization drift because it isn't worth
         * tracking step by step */

        pi >>= shift;
        qi >>= shift;
        qexp += shift - 7 * m;

        pi = ((pi * pi) >> 16);
        qi = ((qi * qi) >> 16);
        qexp = qexp * 2 + m;

        pi *= (1 << 14) - wi;
        qi *= (1 << 14) + wi;
        qi = (qi + pi) >> 14;
      }

      /* we've let the normalization drift because it wasn't important; however,
   for the lookup, things must be normalized again. We need at most one right
   shift or a number of left shifts */

      if (qi & 0xffff0000) { /* checks for 1.xxxxxxxxxxxxxxxx */
        qi >>= 1;
        qexp++;
      } else
        while (qi &&
               !(qi & 0x8000)) { /* checks for 0.0xxxxxxxxxxxxxxx or less*/
          qi <<= 1;
          qexp;
        }

      amp = fromdBlook_i(ampi * /*  n.4         */
                             invsqlook_i(qi, qexp) -
                         /*  m.8, m+n<=8 */
                         ampoffseti); /*  8.12[0]     */

      curve[i] = MULT31_SHIFT15(curve[i], amp);

      while (++i < n) {
        /* line plot to get new f */
        ferr += fdy;
        if (ferr >= fdx) {
          ferr -= fdx;
          f++;
        }
        f += fbase;

        if (f >= nextf) break;

        curve[i] = MULT31_SHIFT15(curve[i], amp);
      }

      while (1) {
        map++;

        if (map + 1 < ln) {
          nextbark = MULT31((map + 1) * (imap >> 1), tBnyq1);

          nextf = barklook[nextbark >> 14] +
                  (((nextbark & 0x3fff) * (barklook[(nextbark >> 14) + 1] -
                                           barklook[nextbark >> 14])) >>
                   14);
          if (f <= nextf) break;
        } else {
          nextf = 9999999;
          break;
        }
      }
      if (map >= ln) {
        map = ln - 1; /* guard against the approximation */
        nextf = 9999999;
      }
    }
  }
  //-
  /* used in init only; interpolate the int32_t way */
  int32_t toBARK(int32_t n) {
    int32_t i;
    for (i = 0; i < 54; i++)
      if (n >= barklook[i] && n < barklook[i + 1]) break;

    if (i == 54) {
      return 54 << 14;
    } else {
      return (i << 14) + (((n - barklook[i]) *
                           ((1UL << 31) / (barklook[i + 1] - barklook[i]))) >>
                          17);
    }
  }
  //-
  /* interpolated lookup based cos function, domain 0 to PI only */
  /* a is in 0.16 format, where 0==0, 2^^16-1==PI, return 0.14 */
  int32_t coslook_i(int32_t a) {
    int32_t i = a >> COS_LOOKUP_I_SHIFT;
    int32_t d = a & COS_LOOKUP_I_MASK;
    return COS_LOOKUP_I[i] - ((d * (COS_LOOKUP_I[i] - COS_LOOKUP_I[i + 1])) >>
                              COS_LOOKUP_I_SHIFT);
  }
  //-
  /* interpolated half-wave lookup based cos function */
  /* a is in 0.16 format, where 0==0, 2^^16==PI, return .LSP_FRACBITS */
  int32_t coslook2_i(int32_t a) {
    int32_t i = a >> COS_LOOKUP_I_SHIFT;
    int32_t d = a & COS_LOOKUP_I_MASK;
    return ((COS_LOOKUP_I[i] << COS_LOOKUP_I_SHIFT) -
            d * (COS_LOOKUP_I[i] - COS_LOOKUP_I[i + 1])) >>
           (COS_LOOKUP_I_SHIFT - LSP_FRACBITS + 14);
  }
  //-
  /* interpolated lookup based fromdB function, domain -140dB to 0dB only */
  /* a is in n.12 format */

  int32_t fromdBlook_i(int32_t a) {
    if (a > 0) return 0x7fffffff;
    if (a < -573440)
      return 0;  // replacement for if(a < (-140 << 12)) return 0;
    return FLOOR_fromdB_LOOKUP[((a + (140 << 12)) * 467) >> 20];
  }
  //-
  int32_t invsqlook_i(int32_t a, int32_t e) {
    int32_t i = (a & 0x7fff) >> (INVSQ_LOOKUP_I_SHIFT - 1);
    int32_t d = a & INVSQ_LOOKUP_I_MASK; /*  0.10 */
    int32_t val =
        INVSQ_LOOKUP_I[i] -                                   /*  1.16 */
        ((INVSQ_LOOKUP_IDel[i] * d) >> INVSQ_LOOKUP_I_SHIFT); /* result 1.16 */
    val *= ADJUST_SQRT2[e & 1];
    e = (e >> 1) + 21;
    return (val >> e);
  }
  //-
  /* partial; doesn't perform last-step deinterleave/unrolling. That can be done
   * more efficiently during pcm output */
  void mdct_backward(int32_t n, int32_t *in) {
    int32_t shift;
    int32_t step;

    for (shift = 4; !(n & (1 << shift)); shift++);
    shift = 13 - shift;
    step = 2 << shift;

    presymmetry(in, n >> 1, step);
    mdct_butterflies(in, n >> 1, shift);
    mdct_bitreverse(in, n, shift);
    mdct_step7(in, n, step);
    mdct_step8(in, n, step);
  }
  //-
  void presymmetry(int32_t *in, int32_t n2, int32_t step) {
    int32_t *aX;
    int32_t *bX;
    const int32_t *T;
    int32_t n4 = n2 >> 1;

    aX = in + n2 - 3;
    T = sincos_lookup0;

    do {
      int32_t r0 = aX[0];
      int32_t r2 = aX[2];
      XPROD31(r0, r2, T[0], T[1], &aX[0], &aX[2]);
      T += step;
      aX -= 4;
    } while (aX >= in + n4);
    do {
      int32_t r0 = aX[0];
      int32_t r2 = aX[2];
      XPROD31(r0, r2, T[1], T[0], &aX[0], &aX[2]);
      T -= step;
      aX -= 4;
    } while (aX >= in);

    aX = in + n2 - 4;
    bX = in;
    T = sincos_lookup0;
    do {
      int32_t ri0 = aX[0];
      int32_t ri2 = aX[2];
      int32_t ro0 = bX[0];
      int32_t ro2 = bX[2];

      XNPROD31(ro2, ro0, T[1], T[0], &aX[0], &aX[2]);
      T += step;
      XNPROD31(ri2, ri0, T[0], T[1], &bX[0], &bX[2]);

      aX -= 4;
      bX += 4;
    } while (aX >= in + n4);
  }
  //-
  void mdct_butterflies(int32_t *x, int32_t points, int32_t shift) {
    int32_t stages = 8 - shift;
    int32_t i, j;

    for (i = 0; stages > 0; i++) {
      for (j = 0; j < (1 << i); j++)
        mdct_butterfly_generic(x + (points >> i) * j, points >> i,
                               4 << (i + shift));
    }

    for (j = 0; j < points; j += 32) mdct_butterfly_32(x + j);
  }
  //-
  /* N/stage point generic N stage butterfly (in place, 2 register) */
  void mdct_butterfly_generic(int32_t *x, int32_t points, int32_t step) {
    const int32_t *T = sincos_lookup0;
    int32_t *x1 = x + points - 4;
    int32_t *x2 = x + (points >> 1) - 4;
    int32_t r0, r1, r2, r3;

    do {
      r0 = x1[0] - x1[1];
      x1[0] += x1[1];
      r1 = x1[3] - x1[2];
      x1[2] += x1[3];
      r2 = x2[1] - x2[0];
      x1[1] = x2[1] + x2[0];
      r3 = x2[3] - x2[2];
      x1[3] = x2[3] + x2[2];
      XPROD31(r1, r0, T[0], T[1], &x2[0], &x2[2]);
      XPROD31(r2, r3, T[0], T[1], &x2[1], &x2[3]);
      T += step;
      x1 -= 4;
      x2 -= 4;
    } while (T < sincos_lookup0 + 1024);
    do {
      r0 = x1[0] - x1[1];
      x1[0] += x1[1];
      r1 = x1[2] - x1[3];
      x1[2] += x1[3];
      r2 = x2[0] - x2[1];
      x1[1] = x2[1] + x2[0];
      r3 = x2[3] - x2[2];
      x1[3] = x2[3] + x2[2];
      XNPROD31(r0, r1, T[0], T[1], &x2[0], &x2[2]);
      XNPROD31(r3, r2, T[0], T[1], &x2[1], &x2[3]);
      T -= step;
      x1 -= 4;
      x2 -= 4;
    } while (T > sincos_lookup0);
  }
  //-
  /* 32 point butterfly (in place, 4 register) */
  void mdct_butterfly_32(int32_t *x) {
    int32_t r0, r1, r2, r3;

    r0 = x[16] - x[17];
    x[16] += x[17];
    r1 = x[18] - x[19];
    x[18] += x[19];
    r2 = x[1] - x[0];
    x[17] = x[1] + x[0];
    r3 = x[3] - x[2];
    x[19] = x[3] + x[2];
    XNPROD31(r0, r1, cPI3_8, cPI1_8, &x[0], &x[2]);
    XPROD31(r2, r3, cPI1_8, cPI3_8, &x[1], &x[3]);

    r0 = x[20] - x[21];
    x[20] += x[21];
    r1 = x[22] - x[23];
    x[22] += x[23];
    r2 = x[5] - x[4];
    x[21] = x[5] + x[4];
    r3 = x[7] - x[6];
    x[23] = x[7] + x[6];
    x[4] = MULT31((r0 - r1), cPI2_8);
    x[5] = MULT31((r3 + r2), cPI2_8);
    x[6] = MULT31((r0 + r1), cPI2_8);
    x[7] = MULT31((r3 - r2), cPI2_8);

    r0 = x[24] - x[25];
    x[24] += x[25];
    r1 = x[26] - x[27];
    x[26] += x[27];
    r2 = x[9] - x[8];
    x[25] = x[9] + x[8];
    r3 = x[11] - x[10];
    x[27] = x[11] + x[10];
    XNPROD31(r0, r1, cPI1_8, cPI3_8, &x[8], &x[10]);
    XPROD31(r2, r3, cPI3_8, cPI1_8, &x[9], &x[11]);

    r0 = x[28] - x[29];
    x[28] += x[29];
    r1 = x[30] - x[31];
    x[30] += x[31];
    r2 = x[12] - x[13];
    x[29] = x[13] + x[12];
    r3 = x[15] - x[14];
    x[31] = x[15] + x[14];
    x[12] = r0;
    x[13] = r3;
    x[14] = r1;
    x[15] = r2;

    mdct_butterfly_16(x);
    mdct_butterfly_16(x + 16);
  }
  //-
  /* 16 point butterfly (in place, 4 register) */
  void mdct_butterfly_16(int32_t *x) {
    int32_t r0, r1, r2, r3;

    r0 = x[8] - x[9];
    x[8] += x[9];
    r1 = x[10] - x[11];
    x[10] += x[11];
    r2 = x[1] - x[0];
    x[9] = x[1] + x[0];
    r3 = x[3] - x[2];
    x[11] = x[3] + x[2];
    x[0] = MULT31((r0 - r1), cPI2_8);
    x[1] = MULT31((r2 + r3), cPI2_8);
    x[2] = MULT31((r0 + r1), cPI2_8);
    x[3] = MULT31((r3 - r2), cPI2_8);

    r2 = x[12] - x[13];
    x[12] += x[13];
    r3 = x[14] - x[15];
    x[14] += x[15];
    r0 = x[4] - x[5];
    x[13] = x[5] + x[4];
    r1 = x[7] - x[6];
    x[15] = x[7] + x[6];
    x[4] = r2;
    x[5] = r1;
    x[6] = r3;
    x[7] = r0;

    mdct_butterfly_8(x);
    mdct_butterfly_8(x + 8);
  }
  //-
  /* 8 point butterfly (in place) */
  void mdct_butterfly_8(int32_t *x) {
    int32_t r0 = x[0] + x[1];
    int32_t r1 = x[0] - x[1];
    int32_t r2 = x[2] + x[3];
    int32_t r3 = x[2] - x[3];
    int32_t r4 = x[4] + x[5];
    int32_t r5 = x[4] - x[5];
    int32_t r6 = x[6] + x[7];
    int32_t r7 = x[6] - x[7];

    x[0] = r5 + r3;
    x[1] = r7 - r1;
    x[2] = r5 - r3;
    x[3] = r7 + r1;
    x[4] = r4 - r0;
    x[5] = r6 - r2;
    x[6] = r4 + r0;
    x[7] = r6 + r2;
  }
  //-
  void mdct_bitreverse(int32_t *x, int32_t n, int32_t shift) {
    int32_t bit = 0;
    int32_t *w = x + (n >> 1);

    do {
      int32_t b = bitrev12(bit++);
      int32_t *xx = x + (b >> shift);
      int32_t r;

      w -= 2;

      if (w > xx) {
        r = xx[0];
        xx[0] = w[0];
        w[0] = r;

        r = xx[1];
        xx[1] = w[1];
        w[1] = r;
      }
    } while (w > x);
  }
  //-
  int32_t bitrev12(int32_t x) {
    uint8_t bitrev[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};
    return bitrev[x >> 8] | (bitrev[(x & 0x0f0) >> 4] << 4) |
           (((int32_t)bitrev[x & 0x00f]) << 8);
  }
  //-
  void mdct_step7(int32_t *x, int32_t n, int32_t step) {
    int32_t *w0 = x;
    int32_t *w1 = x + (n >> 1);
    const int32_t *T =
        (step >= 4) ? (sincos_lookup0 + (step >> 1)) : sincos_lookup1;
    const int32_t *Ttop = T + 1024;
    int32_t r0, r1, r2, r3;

    do {
      w1 -= 2;

      r0 = w0[0] + w1[0];
      r1 = w1[1] - w0[1];
      r2 = MULT32(r0, T[1]) + MULT32(r1, T[0]);
      r3 = MULT32(r1, T[1]) - MULT32(r0, T[0]);
      T += step;

      r0 = (w0[1] + w1[1]) >> 1;
      r1 = (w0[0] - w1[0]) >> 1;
      w0[0] = r0 + r2;
      w0[1] = r1 + r3;
      w1[0] = r0 - r2;
      w1[1] = r3 - r1;

      w0 += 2;
    } while (T < Ttop);
    do {
      w1 -= 2;

      r0 = w0[0] + w1[0];
      r1 = w1[1] - w0[1];
      T -= step;
      r2 = MULT32(r0, T[0]) + MULT32(r1, T[1]);
      r3 = MULT32(r1, T[0]) - MULT32(r0, T[1]);

      r0 = (w0[1] + w1[1]) >> 1;
      r1 = (w0[0] - w1[0]) >> 1;
      w0[0] = r0 + r2;
      w0[1] = r1 + r3;
      w1[0] = r0 - r2;
      w1[1] = r3 - r1;

      w0 += 2;
    } while (w0 < w1);
  }
  //-
  void mdct_step8(int32_t *x, int32_t n, int32_t step) {
    const int32_t *T;
    const int32_t *V;
    int32_t *iX = x + (n >> 1);
    step >>= 2;

    switch (step) {
      default:
        T = (step >= 4) ? (sincos_lookup0 + (step >> 1)) : sincos_lookup1;
        do {
          int32_t r0 = x[0];
          int32_t r1 = -x[1];
          XPROD31(r0, r1, T[0], T[1], x, x + 1);
          T += step;
          x += 2;
        } while (x < iX);
        break;

      case 1: {
        /* linear interpolation between table values: offset=0.5, step=1 */
        int32_t t0, t1, v0, v1, r0, r1;
        T = sincos_lookup0;
        V = sincos_lookup1;
        t0 = (*T++) >> 1;
        t1 = (*T++) >> 1;
        do {
          r0 = x[0];
          r1 = -x[1];
          t0 += (v0 = (*V++) >> 1);
          t1 += (v1 = (*V++) >> 1);
          XPROD31(r0, r1, t0, t1, x, x + 1);

          r0 = x[2];
          r1 = -x[3];
          v0 += (t0 = (*T++) >> 1);
          v1 += (t1 = (*T++) >> 1);
          XPROD31(r0, r1, v0, v1, x + 2, x + 3);

          x += 4;
        } while (x < iX);
        break;
      }

      case 0: {
        /* linear interpolation between table values: offset=0.25, step=0.5 */
        int32_t t0, t1, v0, v1, q0, q1, r0, r1;
        T = sincos_lookup0;
        V = sincos_lookup1;
        t0 = *T++;
        t1 = *T++;
        do {
          v0 = *V++;
          v1 = *V++;
          t0 += (q0 = (v0 - t0) >> 2);
          t1 += (q1 = (v1 - t1) >> 2);
          r0 = x[0];
          r1 = -x[1];
          XPROD31(r0, r1, t0, t1, x, x + 1);
          t0 = v0 - q0;
          t1 = v1 - q1;
          r0 = x[2];
          r1 = -x[3];
          XPROD31(r0, r1, t0, t1, x + 2, x + 3);

          t0 = *T++;
          t1 = *T++;
          v0 += (q0 = (t0 - v0) >> 2);
          v1 += (q1 = (t1 - v1) >> 2);
          r0 = x[4];
          r1 = -x[5];
          XPROD31(r0, r1, v0, v1, x + 4, x + 5);
          v0 = t0 - q0;
          v1 = t1 - q1;
          r0 = x[6];
          r1 = -x[7];
          XPROD31(r0, r1, v0, v1, x + 5, x + 6);

          x += 8;
        } while (x < iX);
        break;
      }
    }
  }
  //-
  /* decode vector / dim granularity guarding is done in the upper layer */
  int32_t book_decodevv_add(codebook_t *book, int32_t **a, int32_t offset,
                            uint8_t ch, int32_t n, int32_t point) {
    if (book->used_entries > 0) {
      int32_t *v = (int32_t *)alloca(sizeof(*v) * book->dim);
      int32_t i;
      uint8_t chptr = 0;
      int32_t m = offset + n;

      for (i = offset; i < m;) {
        if (decode_map(book, v, point)) return -1;
        for (uint8_t j = 0; i < m && j < book->dim; j++) {
          a[chptr++][i] += v[j];
          if (chptr == ch) {
            chptr = 0;
            i++;
          }
        }
      }
    }

    return 0;
  }
  //-
  /* pcm==0 indicates we just want the pending samples, no more */
  int32_t dsp_pcmout(int16_t *outBuff, int32_t outBuffSize) {
    if (s_dsp_state->out_begin > -1 &&
        s_dsp_state->out_begin < s_dsp_state->out_end) {
      int32_t n = s_dsp_state->out_end - s_dsp_state->out_begin;

      if (outBuff) {
        int32_t i;
        if (n > outBuffSize) {
          n = outBuffSize;
          LOGE("outBufferSize too small, must be min %i (int16_t) words", n);
        }
        for (i = 0; i < s_vorbisChannels; i++) {
          mdct_unroll_lap(
              s_blocksizes[0], s_blocksizes[1], s_dsp_state->lW, s_dsp_state->W,
              s_dsp_state->work[i], s_dsp_state->mdctright[i],
              window(s_blocksizes[0] >> 1), window(s_blocksizes[1] >> 1),
              outBuff + i, s_vorbisChannels, s_dsp_state->out_begin,
              s_dsp_state->out_begin + n);
        }
      }
      return (n);
    }
    return (0);
  }
  //-
  int32_t *_window(int32_t left) {
    switch (left) {
      case 32:
        return (int32_t *)vwin64;
      case 64:
        return (int32_t *)vwin128;
      case 128:
        return (int32_t *)vwin256;
      case 256:
        return (int32_t *)vwin512;
      case 512:
        return (int32_t *)vwin1024;
      case 1024:
        return (int32_t *)vwin2048;
      case 2048:
        return (int32_t *)vwin4096;
      case 4096:
        return (int32_t *)vwin8192;
      default:
        return (0);
    }
  }
  //-
  void mdct_unroll_lap(int32_t n0, int32_t n1, int32_t lW, int32_t W,
                       int32_t *in, int32_t *right, const int32_t *w0,
                       const int32_t *w1, int16_t *out, int32_t step,
                       int32_t start, /* samples, this frame */
                       int32_t end /* samples, this frame */) {
    int32_t *l = in + (W && lW ? n1 >> 1 : n0 >> 1);
    int32_t *r = right + (lW ? n1 >> 2 : n0 >> 2);
    int32_t *post;
    const int32_t *wR = (W && lW ? w1 + (n1 >> 1) : w0 + (n0 >> 1));
    const int32_t *wL = (W && lW ? w1 : w0);

    int32_t preLap = (lW && !W ? (n1 >> 2) - (n0 >> 2) : 0);
    int32_t halfLap = (lW && W ? (n1 >> 2) : (n0 >> 2));
    int32_t postLap = (!lW && W ? (n1 >> 2) - (n0 >> 2) : 0);
    int32_t n, off;

    /* preceeding direct-copy lapping from previous frame, if any */
    if (preLap) {
      n = (end < preLap ? end : preLap);
      off = (start < preLap ? start : preLap);
      post = r - n;
      r -= off;
      start -= off;
      end -= n;
      while (r > post) {
        *out = CLIP_TO_15((*r) >> 9);
        out += step;
      }
    }

    /* cross-lap; two halves due to wrap-around */
    n = (end < halfLap ? end : halfLap);
    off = (start < halfLap ? start : halfLap);
    post = r - n;
    r -= off;
    l -= off * 2;
    start -= off;
    wR -= off;
    wL += off;
    end -= n;
    while (r > post) {
      l -= 2;
      *out = CLIP_TO_15((MULT31(*r, *wR) + MULT31(*l, *wL++)) >> 9);
      out += step;
    }

    n = (end < halfLap ? end : halfLap);
    off = (start < halfLap ? start : halfLap);
    post = r + n;
    r += off;
    l += off * 2;
    start -= off;
    end -= n;
    wR -= off;
    wL += off;
    while (r < post) {
      *out = CLIP_TO_15((MULT31(*r++, *wR) - MULT31(*l, *wL++)) >> 9);
      out += step;
      l += 2;
    }

    /* preceeding direct-copy lapping from previous frame, if any */
    if (postLap) {
      n = (end < postLap ? end : postLap);
      off = (start < postLap ? start : postLap);
      post = l + n * 2;
      l += off * 2;
      while (l < post) {
        *out = CLIP_TO_15((-*l) >> 9);
        out += step;
        l += 2;
      }
    }
  }
};

const int32_t DecoderVorbisNative::FLOOR_fromdB_LOOKUP[256] = {
    0x000000e5, 0x000000f4, 0x00000103, 0x00000114, 0x00000126, 0x00000139,
    0x0000014e, 0x00000163, 0x0000017a, 0x00000193, 0x000001ad, 0x000001c9,
    0x000001e7, 0x00000206, 0x00000228, 0x0000024c, 0x00000272, 0x0000029b,
    0x000002c6, 0x000002f4, 0x00000326, 0x0000035a, 0x00000392, 0x000003cd,
    0x0000040c, 0x00000450, 0x00000497, 0x000004e4, 0x00000535, 0x0000058c,
    0x000005e8, 0x0000064a, 0x000006b3, 0x00000722, 0x00000799, 0x00000818,
    0x0000089e, 0x0000092e, 0x000009c6, 0x00000a69, 0x00000b16, 0x00000bcf,
    0x00000c93, 0x00000d64, 0x00000e43, 0x00000f30, 0x0000102d, 0x0000113a,
    0x00001258, 0x0000138a, 0x000014cf, 0x00001629, 0x0000179a, 0x00001922,
    0x00001ac4, 0x00001c82, 0x00001e5c, 0x00002055, 0x0000226f, 0x000024ac,
    0x0000270e, 0x00002997, 0x00002c4b, 0x00002f2c, 0x0000323d, 0x00003581,
    0x000038fb, 0x00003caf, 0x000040a0, 0x000044d3, 0x0000494c, 0x00004e10,
    0x00005323, 0x0000588a, 0x00005e4b, 0x0000646b, 0x00006af2, 0x000071e5,
    0x0000794c, 0x0000812e, 0x00008993, 0x00009283, 0x00009c09, 0x0000a62d,
    0x0000b0f9, 0x0000bc79, 0x0000c8b9, 0x0000d5c4, 0x0000e3a9, 0x0000f274,
    0x00010235, 0x000112fd, 0x000124dc, 0x000137e4, 0x00014c29, 0x000161bf,
    0x000178bc, 0x00019137, 0x0001ab4a, 0x0001c70e, 0x0001e4a1, 0x0002041f,
    0x000225aa, 0x00024962, 0x00026f6d, 0x000297f0, 0x0002c316, 0x0002f109,
    0x000321f9, 0x00035616, 0x00038d97, 0x0003c8b4, 0x000407a7, 0x00044ab2,
    0x00049218, 0x0004de23, 0x00052f1e, 0x0005855c, 0x0005e135, 0x00064306,
    0x0006ab33, 0x00071a24, 0x0007904b, 0x00080e20, 0x00089422, 0x000922da,
    0x0009bad8, 0x000a5cb6, 0x000b091a, 0x000bc0b1, 0x000c8436, 0x000d5471,
    0x000e3233, 0x000f1e5f, 0x001019e4, 0x001125c1, 0x00124306, 0x001372d5,
    0x0014b663, 0x00160ef7, 0x00177df0, 0x001904c1, 0x001aa4f9, 0x001c603d,
    0x001e384f, 0x00202f0f, 0x0022467a, 0x002480b1, 0x0026dff7, 0x002966b3,
    0x002c1776, 0x002ef4fc, 0x0032022d, 0x00354222, 0x0038b828, 0x003c67c2,
    0x004054ae, 0x004482e8, 0x0048f6af, 0x004db488, 0x0052c142, 0x005821ff,
    0x005ddc33, 0x0063f5b0, 0x006a74a7, 0x00715faf, 0x0078bdce, 0x0080967f,
    0x0088f1ba, 0x0091d7f9, 0x009b5247, 0x00a56a41, 0x00b02a27, 0x00bb9ce2,
    0x00c7ce12, 0x00d4ca17, 0x00e29e20, 0x00f15835, 0x0101074b, 0x0111bb4e,
    0x01238531, 0x01367704, 0x014aa402, 0x016020a7, 0x017702c3, 0x018f6190,
    0x01a955cb, 0x01c4f9cf, 0x01e269a8, 0x0201c33b, 0x0223265a, 0x0246b4ea,
    0x026c9302, 0x0294e716, 0x02bfda13, 0x02ed9793, 0x031e4e09, 0x03522ee4,
    0x03896ed0, 0x03c445e2, 0x0402efd6, 0x0445ac4b, 0x048cbefc, 0x04d87013,
    0x05290c67, 0x057ee5ca, 0x05da5364, 0x063bb204, 0x06a36485, 0x0711d42b,
    0x0787710e, 0x0804b299, 0x088a17ef, 0x0918287e, 0x09af747c, 0x0a50957e,
    0x0afc2f19, 0x0bb2ef7f, 0x0c759034, 0x0d44d6ca, 0x0e2195bc, 0x0f0cad0d,
    0x10070b62, 0x1111aeea, 0x122da66c, 0x135c120f, 0x149e24d9, 0x15f525b1,
    0x176270e3, 0x18e7794b, 0x1a85c9ae, 0x1c3f06d1, 0x1e14f07d, 0x200963d7,
    0x221e5ccd, 0x2455f870, 0x26b2770b, 0x29363e2b, 0x2be3db5c, 0x2ebe06b6,
    0x31c7a55b, 0x3503ccd4, 0x3875c5aa, 0x3c210f44, 0x4009632b, 0x4432b8cf,
    0x48a149bc, 0x4d59959e, 0x52606733, 0x57bad899, 0x5d6e593a, 0x6380b298,
    0x69f80e9a, 0x70dafda8, 0x78307d76, 0x7fffffff,
};

const int32_t DecoderVorbisNative::vwin64[32] = {
    0x001f0003, 0x01168c98, 0x030333c8, 0x05dfe3a4, 0x09a49562, 0x0e45df18,
    0x13b47ef2, 0x19dcf676, 0x20a74d83, 0x27f7137c, 0x2fabb05a, 0x37a1105a,
    0x3fb0ab28, 0x47b2dcd1, 0x4f807bc6, 0x56f48e70, 0x5dedfc79, 0x64511653,
    0x6a08cfff, 0x6f079328, 0x734796f4, 0x76cab7f2, 0x7999d6e8, 0x7bc3cf9f,
    0x7d5c20c1, 0x7e7961df, 0x7f33a567, 0x7fa2e1d0, 0x7fdd78a5, 0x7ff6ec6d,
    0x7ffed0e9, 0x7ffffc3f,
};

const  int32_t DecoderVorbisNative::vwin128[64] = {
    0x0007c04d, 0x0045bb89, 0x00c18b87, 0x017ae294, 0x02714a4e, 0x03a4217a,
    0x05129952, 0x06bbb24f, 0x089e38a1, 0x0ab8c073, 0x0d09a228, 0x0f8ef6bd,
    0x12469488, 0x152e0c7a, 0x1842a81c, 0x1b81686d, 0x1ee705d9, 0x226ff15d,
    0x26185705, 0x29dc21cc, 0x2db700fe, 0x31a46f08, 0x359fb9c1, 0x39a40c0c,
    0x3dac78b6, 0x41b40674, 0x45b5bcb0, 0x49acb109, 0x4d94152b, 0x516744bd,
    0x5521d320, 0x58bf98a5, 0x5c3cbef4, 0x5f95cc5d, 0x62c7add7, 0x65cfbf64,
    0x68abd2ba, 0x6b5a3405, 0x6dd9acab, 0x7029840d, 0x72497e38, 0x7439d8ac,
    0x75fb4532, 0x778ee30a, 0x78f6367e, 0x7a331f1a, 0x7b47cccd, 0x7c36b416,
    0x7d028192, 0x7dae0d18, 0x7e3c4caa, 0x7eb04763, 0x7f0d08a7, 0x7f5593b7,
    0x7f8cd7d5, 0x7fb5a513, 0x7fd2a1fc, 0x7fe64212, 0x7ff2bd4c, 0x7ffa0890,
    0x7ffdcf39, 0x7fff6dac, 0x7fffed01, 0x7fffffc4,
};

const int32_t DecoderVorbisNative::vwin256[128] = {
    0x0001f018, 0x00117066, 0x00306e9e, 0x005ee5f1, 0x009ccf26, 0x00ea208b,
    0x0146cdea, 0x01b2c87f, 0x022dfedf, 0x02b85ced, 0x0351cbbd, 0x03fa317f,
    0x04b17167, 0x05776b90, 0x064bfcdc, 0x072efedd, 0x082047b4, 0x091fa9f1,
    0x0a2cf477, 0x0b47f25d, 0x0c706ad2, 0x0da620ff, 0x0ee8d3ef, 0x10383e75,
    0x11941716, 0x12fc0ff6, 0x146fd6c8, 0x15ef14c2, 0x17796e8e, 0x190e844f,
    0x1aadf196, 0x1c574d6e, 0x1e0a2a62, 0x1fc61688, 0x218a9b9c, 0x23573f12,
    0x252b823d, 0x2706e269, 0x28e8d913, 0x2ad0dc0e, 0x2cbe5dc1, 0x2eb0cd60,
    0x30a79733, 0x32a224d5, 0x349fdd8b, 0x36a02690, 0x38a2636f, 0x3aa5f65e,
    0x3caa409e, 0x3eaea2df, 0x40b27da6, 0x42b531b8, 0x44b62086, 0x46b4ac99,
    0x48b03a05, 0x4aa82ed5, 0x4c9bf37d, 0x4e8af349, 0x50749ccb, 0x52586246,
    0x5435ba1c, 0x560c1f31, 0x57db1152, 0x59a21591, 0x5b60b6a3, 0x5d168535,
    0x5ec31839, 0x60660d36, 0x61ff0886, 0x638db595, 0x6511c717, 0x668af734,
    0x67f907b0, 0x695bc207, 0x6ab2f787, 0x6bfe815a, 0x6d3e4090, 0x6e721e16,
    0x6f9a0ab5, 0x70b5fef8, 0x71c5fb16, 0x72ca06cd, 0x73c2313d, 0x74ae90b2,
    0x758f4275, 0x76646a85, 0x772e335c, 0x77eccda0, 0x78a06fd7, 0x79495613,
    0x79e7c19c, 0x7a7bf894, 0x7b064596, 0x7b86f757, 0x7bfe6044, 0x7c6cd615,
    0x7cd2b16e, 0x7d304d71, 0x7d860756, 0x7dd43e06, 0x7e1b51ad, 0x7e5ba355,
    0x7e95947e, 0x7ec986bb, 0x7ef7db4a, 0x7f20f2b9, 0x7f452c7f, 0x7f64e6a7,
    0x7f807d71, 0x7f984aff, 0x7faca700, 0x7fbde662, 0x7fcc5b04, 0x7fd85372,
    0x7fe21a99, 0x7fe9f791, 0x7ff02d58, 0x7ff4fa9e, 0x7ff89990, 0x7ffb3faa,
    0x7ffd1d8b, 0x7ffe5ecc, 0x7fff29e0, 0x7fff9ff3, 0x7fffdcd2, 0x7ffff6d6,
    0x7ffffed0, 0x7ffffffc,
};

const  int32_t DecoderVorbisNative::vwin512[256] = {
    0x00007c06, 0x00045c32, 0x000c1c62, 0x0017bc4c, 0x00273b7a, 0x003a9955,
    0x0051d51c, 0x006cede7, 0x008be2a9, 0x00aeb22a, 0x00d55b0d, 0x00ffdbcc,
    0x012e32b6, 0x01605df5, 0x01965b85, 0x01d02939, 0x020dc4ba, 0x024f2b83,
    0x02945ae6, 0x02dd5004, 0x032a07d3, 0x037a7f19, 0x03ceb26e, 0x04269e37,
    0x04823eab, 0x04e18fcc, 0x05448d6d, 0x05ab3329, 0x06157c68, 0x0683645e,
    0x06f4e607, 0x0769fc25, 0x07e2a146, 0x085ecfbc, 0x08de819f, 0x0961b0cc,
    0x09e856e3, 0x0a726d46, 0x0affed1d, 0x0b90cf4c, 0x0c250c79, 0x0cbc9d0b,
    0x0d577926, 0x0df598aa, 0x0e96f337, 0x0f3b8026, 0x0fe3368f, 0x108e0d42,
    0x113bfaca, 0x11ecf56b, 0x12a0f324, 0x1357e9ac, 0x1411ce70, 0x14ce9698,
    0x158e3702, 0x1650a444, 0x1715d2aa, 0x17ddb638, 0x18a842aa, 0x19756b72,
    0x1a4523b9, 0x1b175e62, 0x1bec0e04, 0x1cc324f0, 0x1d9c9532, 0x1e78508a,
    0x1f564876, 0x20366e2e, 0x2118b2a2, 0x21fd0681, 0x22e35a37, 0x23cb9dee,
    0x24b5c18e, 0x25a1b4c0, 0x268f66f1, 0x277ec74e, 0x286fc4cc, 0x29624e23,
    0x2a5651d7, 0x2b4bbe34, 0x2c428150, 0x2d3a8913, 0x2e33c332, 0x2f2e1d35,
    0x30298478, 0x3125e62d, 0x32232f61, 0x33214cfc, 0x34202bc2, 0x351fb85a,
    0x361fdf4f, 0x37208d10, 0x3821adf7, 0x39232e49, 0x3a24fa3c, 0x3b26fdf6,
    0x3c292593, 0x3d2b5d29, 0x3e2d90c8, 0x3f2fac7f, 0x40319c5f, 0x41334c81,
    0x4234a905, 0x43359e16, 0x443617f3, 0x453602eb, 0x46354b65, 0x4733dde1,
    0x4831a6ff, 0x492e937f, 0x4a2a9045, 0x4b258a5f, 0x4c1f6f06, 0x4d182ba2,
    0x4e0fadce, 0x4f05e35b, 0x4ffaba53, 0x50ee20fd, 0x51e005e1, 0x52d057ca,
    0x53bf05ca, 0x54abff3b, 0x559733c7, 0x56809365, 0x57680e62, 0x584d955d,
    0x59311952, 0x5a128b96, 0x5af1dddd, 0x5bcf023a, 0x5ca9eb27, 0x5d828b81,
    0x5e58d68d, 0x5f2cbffc, 0x5ffe3be9, 0x60cd3edf, 0x6199bdda, 0x6263ae45,
    0x632b0602, 0x63efbb66, 0x64b1c53f, 0x65711ad0, 0x662db3d7, 0x66e7888d,
    0x679e91a5, 0x6852c84e, 0x69042635, 0x69b2a582, 0x6a5e40dd, 0x6b06f36c,
    0x6bacb8d2, 0x6c4f8d30, 0x6cef6d26, 0x6d8c55d4, 0x6e2644d4, 0x6ebd3840,
    0x6f512ead, 0x6fe2272e, 0x7070214f, 0x70fb1d17, 0x71831b06, 0x72081c16,
    0x728a21b5, 0x73092dc8, 0x738542a6, 0x73fe631b, 0x74749261, 0x74e7d421,
    0x75582c72, 0x75c59fd5, 0x76303333, 0x7697ebdd, 0x76fccf85, 0x775ee443,
    0x77be308a, 0x781abb2e, 0x78748b59, 0x78cba88e, 0x79201aa7, 0x7971e9cd,
    0x79c11e79, 0x7a0dc170, 0x7a57dbc2, 0x7a9f76c1, 0x7ae49c07, 0x7b27556b,
    0x7b67ad02, 0x7ba5ad1b, 0x7be1603a, 0x7c1ad118, 0x7c520a9e, 0x7c8717e1,
    0x7cba0421, 0x7ceadac3, 0x7d19a74f, 0x7d46756e, 0x7d7150e5, 0x7d9a4592,
    0x7dc15f69, 0x7de6aa71, 0x7e0a32c0, 0x7e2c0479, 0x7e4c2bc7, 0x7e6ab4db,
    0x7e87abe9, 0x7ea31d24, 0x7ebd14be, 0x7ed59edd, 0x7eecc7a3, 0x7f029b21,
    0x7f17255a, 0x7f2a723f, 0x7f3c8daa, 0x7f4d835d, 0x7f5d5f00, 0x7f6c2c1b,
    0x7f79f617, 0x7f86c83a, 0x7f92ada2, 0x7f9db146, 0x7fa7ddf3, 0x7fb13e46,
    0x7fb9dcb0, 0x7fc1c36c, 0x7fc8fc83, 0x7fcf91c7, 0x7fd58cd2, 0x7fdaf702,
    0x7fdfd979, 0x7fe43d1c, 0x7fe82a8b, 0x7febaa29, 0x7feec412, 0x7ff1801c,
    0x7ff3e5d6, 0x7ff5fc86, 0x7ff7cb29, 0x7ff9586f, 0x7ffaaaba, 0x7ffbc81e,
    0x7ffcb660, 0x7ffd7af3, 0x7ffe1afa, 0x7ffe9b42, 0x7fff0047, 0x7fff4e2f,
    0x7fff88c9, 0x7fffb390, 0x7fffd1a6, 0x7fffe5d7, 0x7ffff296, 0x7ffff9fd,
    0x7ffffdcd, 0x7fffff6d, 0x7fffffed, 0x7fffffff,
};

const  int32_t DecoderVorbisNative::vwin1024[512] = {
    0x00001f02, 0x0001170e, 0x00030724, 0x0005ef40, 0x0009cf59, 0x000ea767,
    0x0014775e, 0x001b3f2e, 0x0022fec8, 0x002bb618, 0x00356508, 0x00400b81,
    0x004ba968, 0x00583ea0, 0x0065cb0a, 0x00744e84, 0x0083c8ea, 0x00943a14,
    0x00a5a1da, 0x00b80010, 0x00cb5488, 0x00df9f10, 0x00f4df76, 0x010b1584,
    0x01224101, 0x013a61b2, 0x01537759, 0x016d81b6, 0x01888087, 0x01a47385,
    0x01c15a69, 0x01df34e6, 0x01fe02b1, 0x021dc377, 0x023e76e7, 0x02601ca9,
    0x0282b466, 0x02a63dc1, 0x02cab85d, 0x02f023d6, 0x03167fcb, 0x033dcbd3,
    0x03660783, 0x038f3270, 0x03b94c29, 0x03e4543a, 0x04104a2e, 0x043d2d8b,
    0x046afdd5, 0x0499ba8c, 0x04c9632d, 0x04f9f734, 0x052b7615, 0x055ddf46,
    0x05913237, 0x05c56e53, 0x05fa9306, 0x06309fb6, 0x066793c5, 0x069f6e93,
    0x06d82f7c, 0x0711d5d9, 0x074c60fe, 0x0787d03d, 0x07c422e4, 0x0801583e,
    0x083f6f91, 0x087e681f, 0x08be4129, 0x08fef9ea, 0x0940919a, 0x0983076d,
    0x09c65a92, 0x0a0a8a38, 0x0a4f9585, 0x0a957b9f, 0x0adc3ba7, 0x0b23d4b9,
    0x0b6c45ee, 0x0bb58e5a, 0x0bffad0f, 0x0c4aa11a, 0x0c966982, 0x0ce3054d,
    0x0d30737b, 0x0d7eb308, 0x0dcdc2eb, 0x0e1da21a, 0x0e6e4f83, 0x0ebfca11,
    0x0f1210ad, 0x0f652238, 0x0fb8fd91, 0x100da192, 0x10630d11, 0x10b93ee0,
    0x111035cb, 0x1167f09a, 0x11c06e13, 0x1219acf5, 0x1273abfb, 0x12ce69db,
    0x1329e54a, 0x13861cf3, 0x13e30f80, 0x1440bb97, 0x149f1fd8, 0x14fe3ade,
    0x155e0b40, 0x15be8f92, 0x161fc662, 0x1681ae38, 0x16e4459b, 0x17478b0b,
    0x17ab7d03, 0x181019fb, 0x18756067, 0x18db4eb3, 0x1941e34a, 0x19a91c92,
    0x1a10f8ea, 0x1a7976af, 0x1ae29439, 0x1b4c4fda, 0x1bb6a7e2, 0x1c219a9a,
    0x1c8d2649, 0x1cf9492e, 0x1d660188, 0x1dd34d8e, 0x1e412b74, 0x1eaf996a,
    0x1f1e959b, 0x1f8e1e2f, 0x1ffe3146, 0x206ecd01, 0x20dfef78, 0x215196c2,
    0x21c3c0f0, 0x22366c10, 0x22a9962a, 0x231d3d45, 0x23915f60, 0x2405fa7a,
    0x247b0c8c, 0x24f09389, 0x25668d65, 0x25dcf80c, 0x2653d167, 0x26cb175e,
    0x2742c7d0, 0x27bae09e, 0x28335fa2, 0x28ac42b3, 0x292587a5, 0x299f2c48,
    0x2a192e69, 0x2a938bd1, 0x2b0e4247, 0x2b894f8d, 0x2c04b164, 0x2c806588,
    0x2cfc69b2, 0x2d78bb9a, 0x2df558f4, 0x2e723f6f, 0x2eef6cbb, 0x2f6cde83,
    0x2fea9270, 0x30688627, 0x30e6b74e, 0x31652385, 0x31e3c86b, 0x3262a39e,
    0x32e1b2b8, 0x3360f352, 0x33e06303, 0x345fff5e, 0x34dfc5f8, 0x355fb462,
    0x35dfc82a, 0x365ffee0, 0x36e0560f, 0x3760cb43, 0x37e15c05, 0x386205df,
    0x38e2c657, 0x39639af5, 0x39e4813e, 0x3a6576b6, 0x3ae678e3, 0x3b678547,
    0x3be89965, 0x3c69b2c1, 0x3ceacedc, 0x3d6beb37, 0x3ded0557, 0x3e6e1abb,
    0x3eef28e6, 0x3f702d5a, 0x3ff1259a, 0x40720f29, 0x40f2e789, 0x4173ac3f,
    0x41f45ad0, 0x4274f0c2, 0x42f56b9a, 0x4375c8e0, 0x43f6061d, 0x447620db,
    0x44f616a5, 0x4575e509, 0x45f58994, 0x467501d6, 0x46f44b62, 0x477363cb,
    0x47f248a6, 0x4870f78e, 0x48ef6e1a, 0x496da9e8, 0x49eba897, 0x4a6967c8,
    0x4ae6e521, 0x4b641e47, 0x4be110e5, 0x4c5dbaa7, 0x4cda193f, 0x4d562a5f,
    0x4dd1ebbd, 0x4e4d5b15, 0x4ec87623, 0x4f433aa9, 0x4fbda66c, 0x5037b734,
    0x50b16acf, 0x512abf0e, 0x51a3b1c5, 0x521c40ce, 0x52946a06, 0x530c2b50,
    0x53838292, 0x53fa6db8, 0x5470eab3, 0x54e6f776, 0x555c91fc, 0x55d1b844,
    0x56466851, 0x56baa02f, 0x572e5deb, 0x57a19f98, 0x58146352, 0x5886a737,
    0x58f8696d, 0x5969a81c, 0x59da6177, 0x5a4a93b4, 0x5aba3d0f, 0x5b295bcb,
    0x5b97ee30, 0x5c05f28d, 0x5c736738, 0x5ce04a8d, 0x5d4c9aed, 0x5db856c1,
    0x5e237c78, 0x5e8e0a89, 0x5ef7ff6f, 0x5f6159b0, 0x5fca17d4, 0x6032386e,
    0x6099ba15, 0x61009b69, 0x6166db11, 0x61cc77b9, 0x62317017, 0x6295c2e7,
    0x62f96eec, 0x635c72f1, 0x63becdc8, 0x64207e4b, 0x6481835a, 0x64e1dbde,
    0x654186c8, 0x65a0830e, 0x65fecfb1, 0x665c6bb7, 0x66b95630, 0x67158e30,
    0x677112d7, 0x67cbe34b, 0x6825feb9, 0x687f6456, 0x68d81361, 0x69300b1e,
    0x69874ada, 0x69ddd1ea, 0x6a339fab, 0x6a88b382, 0x6add0cdb, 0x6b30ab2a,
    0x6b838dec, 0x6bd5b4a6, 0x6c271ee2, 0x6c77cc36, 0x6cc7bc3d, 0x6d16ee9b,
    0x6d6562fb, 0x6db31911, 0x6e001099, 0x6e4c4955, 0x6e97c311, 0x6ee27d9f,
    0x6f2c78d9, 0x6f75b4a2, 0x6fbe30e4, 0x7005ed91, 0x704ceaa1, 0x70932816,
    0x70d8a5f8, 0x711d6457, 0x7161634b, 0x71a4a2f3, 0x71e72375, 0x7228e500,
    0x7269e7c8, 0x72aa2c0a, 0x72e9b209, 0x73287a12, 0x73668476, 0x73a3d18f,
    0x73e061bc, 0x741c3566, 0x74574cfa, 0x7491a8ee, 0x74cb49be, 0x75042fec,
    0x753c5c03, 0x7573ce92, 0x75aa882f, 0x75e08979, 0x7615d313, 0x764a65a7,
    0x767e41e5, 0x76b16884, 0x76e3da40, 0x771597dc, 0x7746a221, 0x7776f9dd,
    0x77a69fe6, 0x77d59514, 0x7803da49, 0x7831706a, 0x785e5861, 0x788a9320,
    0x78b6219c, 0x78e104cf, 0x790b3dbb, 0x7934cd64, 0x795db4d5, 0x7985f51d,
    0x79ad8f50, 0x79d48486, 0x79fad5de, 0x7a208478, 0x7a45917b, 0x7a69fe12,
    0x7a8dcb6c, 0x7ab0fabb, 0x7ad38d36, 0x7af5841a, 0x7b16e0a3, 0x7b37a416,
    0x7b57cfb8, 0x7b7764d4, 0x7b9664b6, 0x7bb4d0b0, 0x7bd2aa14, 0x7beff23b,
    0x7c0caa7f, 0x7c28d43c, 0x7c4470d2, 0x7c5f81a5, 0x7c7a081a, 0x7c940598,
    0x7cad7b8b, 0x7cc66b5e, 0x7cded680, 0x7cf6be64, 0x7d0e247b, 0x7d250a3c,
    0x7d3b711c, 0x7d515a95, 0x7d66c822, 0x7d7bbb3c, 0x7d903563, 0x7da43814,
    0x7db7c4d0, 0x7dcadd16, 0x7ddd826a, 0x7defb64d, 0x7e017a44, 0x7e12cfd3,
    0x7e23b87f, 0x7e3435cc, 0x7e444943, 0x7e53f467, 0x7e6338c0, 0x7e7217d5,
    0x7e80932b, 0x7e8eac49, 0x7e9c64b7, 0x7ea9bdf8, 0x7eb6b994, 0x7ec35910,
    0x7ecf9def, 0x7edb89b6, 0x7ee71de9, 0x7ef25c09, 0x7efd4598, 0x7f07dc16,
    0x7f122103, 0x7f1c15dc, 0x7f25bc1f, 0x7f2f1547, 0x7f3822cd, 0x7f40e62b,
    0x7f4960d6, 0x7f519443, 0x7f5981e7, 0x7f612b31, 0x7f689191, 0x7f6fb674,
    0x7f769b45, 0x7f7d416c, 0x7f83aa51, 0x7f89d757, 0x7f8fc9df, 0x7f958348,
    0x7f9b04ef, 0x7fa0502e, 0x7fa56659, 0x7faa48c7, 0x7faef8c7, 0x7fb377a7,
    0x7fb7c6b3, 0x7fbbe732, 0x7fbfda67, 0x7fc3a196, 0x7fc73dfa, 0x7fcab0ce,
    0x7fcdfb4a, 0x7fd11ea0, 0x7fd41c00, 0x7fd6f496, 0x7fd9a989, 0x7fdc3bff,
    0x7fdead17, 0x7fe0fdee, 0x7fe32f9d, 0x7fe54337, 0x7fe739ce, 0x7fe9146c,
    0x7fead41b, 0x7fec79dd, 0x7fee06b2, 0x7fef7b94, 0x7ff0d97b, 0x7ff22158,
    0x7ff35417, 0x7ff472a3, 0x7ff57de0, 0x7ff676ac, 0x7ff75de3, 0x7ff8345a,
    0x7ff8fae4, 0x7ff9b24b, 0x7ffa5b58, 0x7ffaf6cd, 0x7ffb8568, 0x7ffc07e2,
    0x7ffc7eed, 0x7ffceb38, 0x7ffd4d6d, 0x7ffda631, 0x7ffdf621, 0x7ffe3dd8,
    0x7ffe7dea, 0x7ffeb6e7, 0x7ffee959, 0x7fff15c4, 0x7fff3ca9, 0x7fff5e80,
    0x7fff7bc0, 0x7fff94d6, 0x7fffaa2d, 0x7fffbc29, 0x7fffcb29, 0x7fffd786,
    0x7fffe195, 0x7fffe9a3, 0x7fffeffa, 0x7ffff4dd, 0x7ffff889, 0x7ffffb37,
    0x7ffffd1a, 0x7ffffe5d, 0x7fffff29, 0x7fffffa0, 0x7fffffdd, 0x7ffffff7,
    0x7fffffff, 0x7fffffff,
};

const int32_t DecoderVorbisNative::vwin2048[1024] = {
    0x000007c0, 0x000045c4, 0x0000c1ca, 0x00017bd3, 0x000273de, 0x0003a9eb,
    0x00051df9, 0x0006d007, 0x0008c014, 0x000aee1e, 0x000d5a25, 0x00100428,
    0x0012ec23, 0x00161216, 0x001975fe, 0x001d17da, 0x0020f7a8, 0x00251564,
    0x0029710c, 0x002e0a9e, 0x0032e217, 0x0037f773, 0x003d4ab0, 0x0042dbca,
    0x0048aabe, 0x004eb788, 0x00550224, 0x005b8a8f, 0x006250c5, 0x006954c1,
    0x0070967e, 0x007815f9, 0x007fd32c, 0x0087ce13, 0x009006a9, 0x00987ce9,
    0x00a130cc, 0x00aa224f, 0x00b3516b, 0x00bcbe1a, 0x00c66856, 0x00d0501a,
    0x00da755f, 0x00e4d81f, 0x00ef7853, 0x00fa55f4, 0x010570fc, 0x0110c963,
    0x011c5f22, 0x01283232, 0x0134428c, 0x01409027, 0x014d1afb, 0x0159e302,
    0x0166e831, 0x01742a82, 0x0181a9ec, 0x018f6665, 0x019d5fe5, 0x01ab9663,
    0x01ba09d6, 0x01c8ba34, 0x01d7a775, 0x01e6d18d, 0x01f63873, 0x0205dc1e,
    0x0215bc82, 0x0225d997, 0x02363350, 0x0246c9a3, 0x02579c86, 0x0268abed,
    0x0279f7cc, 0x028b801a, 0x029d44c9, 0x02af45ce, 0x02c1831d, 0x02d3fcaa,
    0x02e6b269, 0x02f9a44c, 0x030cd248, 0x03203c4f, 0x0333e255, 0x0347c44b,
    0x035be225, 0x03703bd5, 0x0384d14d, 0x0399a280, 0x03aeaf5e, 0x03c3f7d9,
    0x03d97be4, 0x03ef3b6e, 0x0405366a, 0x041b6cc8, 0x0431de78, 0x04488b6c,
    0x045f7393, 0x047696dd, 0x048df53b, 0x04a58e9b, 0x04bd62ee, 0x04d57223,
    0x04edbc28, 0x050640ed, 0x051f0060, 0x0537fa70, 0x05512f0a, 0x056a9e1e,
    0x05844798, 0x059e2b67, 0x05b84978, 0x05d2a1b8, 0x05ed3414, 0x06080079,
    0x062306d3, 0x063e470f, 0x0659c119, 0x067574dd, 0x06916247, 0x06ad8941,
    0x06c9e9b8, 0x06e68397, 0x070356c8, 0x07206336, 0x073da8cb, 0x075b2772,
    0x0778df15, 0x0796cf9c, 0x07b4f8f3, 0x07d35b01, 0x07f1f5b1, 0x0810c8eb,
    0x082fd497, 0x084f189e, 0x086e94e9, 0x088e495e, 0x08ae35e6, 0x08ce5a68,
    0x08eeb6cc, 0x090f4af8, 0x093016d3, 0x09511a44, 0x09725530, 0x0993c77f,
    0x09b57115, 0x09d751d8, 0x09f969ae, 0x0a1bb87c, 0x0a3e3e26, 0x0a60fa91,
    0x0a83eda2, 0x0aa7173c, 0x0aca7743, 0x0aee0d9b, 0x0b11da28, 0x0b35dccc,
    0x0b5a156a, 0x0b7e83e5, 0x0ba3281f, 0x0bc801fa, 0x0bed1159, 0x0c12561c,
    0x0c37d025, 0x0c5d7f55, 0x0c83638d, 0x0ca97cae, 0x0ccfca97, 0x0cf64d2a,
    0x0d1d0444, 0x0d43efc7, 0x0d6b0f92, 0x0d926383, 0x0db9eb79, 0x0de1a752,
    0x0e0996ee, 0x0e31ba29, 0x0e5a10e2, 0x0e829af6, 0x0eab5841, 0x0ed448a2,
    0x0efd6bf4, 0x0f26c214, 0x0f504ade, 0x0f7a062e, 0x0fa3f3df, 0x0fce13cd,
    0x0ff865d2, 0x1022e9ca, 0x104d9f8e, 0x107886f9, 0x10a39fe5, 0x10ceea2c,
    0x10fa65a6, 0x1126122d, 0x1151ef9a, 0x117dfdc5, 0x11aa3c87, 0x11d6abb6,
    0x12034b2c, 0x12301ac0, 0x125d1a48, 0x128a499b, 0x12b7a891, 0x12e536ff,
    0x1312f4bb, 0x1340e19c, 0x136efd75, 0x139d481e, 0x13cbc16a, 0x13fa692f,
    0x14293f40, 0x14584371, 0x14877597, 0x14b6d585, 0x14e6630d, 0x15161e04,
    0x1546063b, 0x15761b85, 0x15a65db3, 0x15d6cc99, 0x16076806, 0x16382fcd,
    0x166923bf, 0x169a43ab, 0x16cb8f62, 0x16fd06b5, 0x172ea973, 0x1760776b,
    0x1792706e, 0x17c49449, 0x17f6e2cb, 0x18295bc3, 0x185bfeff, 0x188ecc4c,
    0x18c1c379, 0x18f4e452, 0x19282ea4, 0x195ba23c, 0x198f3ee6, 0x19c3046e,
    0x19f6f2a1, 0x1a2b094a, 0x1a5f4833, 0x1a93af28, 0x1ac83df3, 0x1afcf460,
    0x1b31d237, 0x1b66d744, 0x1b9c034e, 0x1bd15621, 0x1c06cf84, 0x1c3c6f40,
    0x1c72351e, 0x1ca820e6, 0x1cde3260, 0x1d146953, 0x1d4ac587, 0x1d8146c3,
    0x1db7eccd, 0x1deeb76c, 0x1e25a667, 0x1e5cb982, 0x1e93f085, 0x1ecb4b33,
    0x1f02c953, 0x1f3a6aaa, 0x1f722efb, 0x1faa160b, 0x1fe21f9e, 0x201a4b79,
    0x2052995d, 0x208b0910, 0x20c39a53, 0x20fc4cea, 0x21352097, 0x216e151c,
    0x21a72a3a, 0x21e05fb5, 0x2219b54d, 0x22532ac3, 0x228cbfd8, 0x22c6744d,
    0x230047e2, 0x233a3a58, 0x23744b6d, 0x23ae7ae3, 0x23e8c878, 0x242333ec,
    0x245dbcfd, 0x24986369, 0x24d326f1, 0x250e0750, 0x25490446, 0x25841d90,
    0x25bf52ec, 0x25faa417, 0x263610cd, 0x267198cc, 0x26ad3bcf, 0x26e8f994,
    0x2724d1d6, 0x2760c451, 0x279cd0c0, 0x27d8f6e0, 0x2815366a, 0x28518f1b,
    0x288e00ac, 0x28ca8ad8, 0x29072d5a, 0x2943e7eb, 0x2980ba45, 0x29bda422,
    0x29faa53c, 0x2a37bd4a, 0x2a74ec07, 0x2ab2312b, 0x2aef8c6f, 0x2b2cfd8b,
    0x2b6a8437, 0x2ba8202c, 0x2be5d120, 0x2c2396cc, 0x2c6170e7, 0x2c9f5f29,
    0x2cdd6147, 0x2d1b76fa, 0x2d599ff7, 0x2d97dbf5, 0x2dd62aab, 0x2e148bcf,
    0x2e52ff16, 0x2e918436, 0x2ed01ae5, 0x2f0ec2d9, 0x2f4d7bc6, 0x2f8c4562,
    0x2fcb1f62, 0x300a097a, 0x3049035f, 0x30880cc6, 0x30c72563, 0x31064cea,
    0x3145830f, 0x3184c786, 0x31c41a03, 0x32037a39, 0x3242e7dc, 0x3282629f,
    0x32c1ea36, 0x33017e53, 0x33411ea9, 0x3380caec, 0x33c082ce, 0x34004602,
    0x34401439, 0x347fed27, 0x34bfd07e, 0x34ffbdf0, 0x353fb52e, 0x357fb5ec,
    0x35bfbfda, 0x35ffd2aa, 0x363fee0f, 0x368011b9, 0x36c03d5a, 0x370070a4,
    0x3740ab48, 0x3780ecf7, 0x37c13562, 0x3801843a, 0x3841d931, 0x388233f7,
    0x38c2943d, 0x3902f9b4, 0x3943640d, 0x3983d2f8, 0x39c44626, 0x3a04bd48,
    0x3a45380e, 0x3a85b62a, 0x3ac6374a, 0x3b06bb20, 0x3b47415c, 0x3b87c9ae,
    0x3bc853c7, 0x3c08df57, 0x3c496c0f, 0x3c89f99f, 0x3cca87b6, 0x3d0b1605,
    0x3d4ba43d, 0x3d8c320e, 0x3dccbf27, 0x3e0d4b3a, 0x3e4dd5f6, 0x3e8e5f0c,
    0x3ecee62b, 0x3f0f6b05, 0x3f4fed49, 0x3f906ca8, 0x3fd0e8d2, 0x40116177,
    0x4051d648, 0x409246f6, 0x40d2b330, 0x41131aa7, 0x41537d0c, 0x4193da10,
    0x41d43162, 0x421482b4, 0x4254cdb7, 0x4295121b, 0x42d54f91, 0x431585ca,
    0x4355b477, 0x4395db49, 0x43d5f9f1, 0x44161021, 0x44561d8a, 0x449621dd,
    0x44d61ccc, 0x45160e08, 0x4555f544, 0x4595d230, 0x45d5a47f, 0x46156be3,
    0x4655280e, 0x4694d8b2, 0x46d47d82, 0x4714162f, 0x4753a26d, 0x479321ef,
    0x47d29466, 0x4811f987, 0x48515104, 0x48909a91, 0x48cfd5e1, 0x490f02a7,
    0x494e2098, 0x498d2f66, 0x49cc2ec7, 0x4a0b1e6f, 0x4a49fe11, 0x4a88cd62,
    0x4ac78c18, 0x4b0639e6, 0x4b44d683, 0x4b8361a2, 0x4bc1dafa, 0x4c004241,
    0x4c3e972c, 0x4c7cd970, 0x4cbb08c5, 0x4cf924e1, 0x4d372d7a, 0x4d752247,
    0x4db30300, 0x4df0cf5a, 0x4e2e870f, 0x4e6c29d6, 0x4ea9b766, 0x4ee72f78,
    0x4f2491c4, 0x4f61de02, 0x4f9f13ec, 0x4fdc333b, 0x50193ba8, 0x50562ced,
    0x509306c3, 0x50cfc8e5, 0x510c730d, 0x514904f6, 0x51857e5a, 0x51c1def5,
    0x51fe2682, 0x523a54bc, 0x52766961, 0x52b2642c, 0x52ee44d9, 0x532a0b26,
    0x5365b6d0, 0x53a14793, 0x53dcbd2f, 0x54181760, 0x545355e5, 0x548e787d,
    0x54c97ee6, 0x550468e1, 0x553f362c, 0x5579e687, 0x55b479b3, 0x55eeef70,
    0x5629477f, 0x566381a1, 0x569d9d97, 0x56d79b24, 0x57117a0a, 0x574b3a0a,
    0x5784dae9, 0x57be5c69, 0x57f7be4d, 0x5831005a, 0x586a2254, 0x58a32400,
    0x58dc0522, 0x5914c57f, 0x594d64de, 0x5985e305, 0x59be3fba, 0x59f67ac3,
    0x5a2e93e9, 0x5a668af2, 0x5a9e5fa6, 0x5ad611ce, 0x5b0da133, 0x5b450d9d,
    0x5b7c56d7, 0x5bb37ca9, 0x5bea7ede, 0x5c215d41, 0x5c58179d, 0x5c8eadbe,
    0x5cc51f6f, 0x5cfb6c7c, 0x5d3194b2, 0x5d6797de, 0x5d9d75cf, 0x5dd32e51,
    0x5e08c132, 0x5e3e2e43, 0x5e737551, 0x5ea8962d, 0x5edd90a7, 0x5f12648e,
    0x5f4711b4, 0x5f7b97ea, 0x5faff702, 0x5fe42ece, 0x60183f20, 0x604c27cc,
    0x607fe8a6, 0x60b38180, 0x60e6f22f, 0x611a3a89, 0x614d5a62, 0x61805190,
    0x61b31fe9, 0x61e5c545, 0x62184179, 0x624a945d, 0x627cbdca, 0x62aebd98,
    0x62e0939f, 0x63123fba, 0x6343c1c1, 0x6375198f, 0x63a646ff, 0x63d749ec,
    0x64082232, 0x6438cfad, 0x64695238, 0x6499a9b3, 0x64c9d5f9, 0x64f9d6ea,
    0x6529ac63, 0x65595643, 0x6588d46a, 0x65b826b8, 0x65e74d0e, 0x6616474b,
    0x66451552, 0x6673b704, 0x66a22c44, 0x66d074f4, 0x66fe90f8, 0x672c8033,
    0x675a428a, 0x6787d7e1, 0x67b5401f, 0x67e27b27, 0x680f88e1, 0x683c6934,
    0x68691c05, 0x6895a13e, 0x68c1f8c7, 0x68ee2287, 0x691a1e68, 0x6945ec54,
    0x69718c35, 0x699cfdf5, 0x69c8417f, 0x69f356c0, 0x6a1e3da3, 0x6a48f615,
    0x6a738002, 0x6a9ddb5a, 0x6ac80808, 0x6af205fd, 0x6b1bd526, 0x6b457575,
    0x6b6ee6d8, 0x6b982940, 0x6bc13c9f, 0x6bea20e5, 0x6c12d605, 0x6c3b5bf1,
    0x6c63b29c, 0x6c8bd9fb, 0x6cb3d200, 0x6cdb9aa0, 0x6d0333d0, 0x6d2a9d86,
    0x6d51d7b7, 0x6d78e25a, 0x6d9fbd67, 0x6dc668d3, 0x6dece498, 0x6e1330ad,
    0x6e394d0c, 0x6e5f39ae, 0x6e84f68d, 0x6eaa83a2, 0x6ecfe0ea, 0x6ef50e5e,
    0x6f1a0bfc, 0x6f3ed9bf, 0x6f6377a4, 0x6f87e5a8, 0x6fac23c9, 0x6fd03206,
    0x6ff4105c, 0x7017becc, 0x703b3d54, 0x705e8bf5, 0x7081aaaf, 0x70a49984,
    0x70c75874, 0x70e9e783, 0x710c46b2, 0x712e7605, 0x7150757f, 0x71724523,
    0x7193e4f6, 0x71b554fd, 0x71d6953e, 0x71f7a5bd, 0x72188681, 0x72393792,
    0x7259b8f5, 0x727a0ab2, 0x729a2cd2, 0x72ba1f5d, 0x72d9e25c, 0x72f975d8,
    0x7318d9db, 0x73380e6f, 0x735713a0, 0x7375e978, 0x73949003, 0x73b3074c,
    0x73d14f61, 0x73ef684f, 0x740d5222, 0x742b0ce9, 0x744898b1, 0x7465f589,
    0x74832381, 0x74a022a8, 0x74bcf30e, 0x74d994c3, 0x74f607d8, 0x75124c5f,
    0x752e6268, 0x754a4a05, 0x7566034b, 0x75818e4a, 0x759ceb16, 0x75b819c4,
    0x75d31a66, 0x75eded12, 0x760891dc, 0x762308da, 0x763d5221, 0x76576dc8,
    0x76715be4, 0x768b1c8c, 0x76a4afd9, 0x76be15e0, 0x76d74ebb, 0x76f05a82,
    0x7709394d, 0x7721eb35, 0x773a7054, 0x7752c8c4, 0x776af49f, 0x7782f400,
    0x779ac701, 0x77b26dbd, 0x77c9e851, 0x77e136d8, 0x77f8596f, 0x780f5032,
    0x78261b3f, 0x783cbab2, 0x78532eaa, 0x78697745, 0x787f94a0, 0x789586db,
    0x78ab4e15, 0x78c0ea6d, 0x78d65c03, 0x78eba2f7, 0x7900bf68, 0x7915b179,
    0x792a7949, 0x793f16fb, 0x79538aaf, 0x7967d488, 0x797bf4a8, 0x798feb31,
    0x79a3b846, 0x79b75c0a, 0x79cad6a1, 0x79de282e, 0x79f150d5, 0x7a0450bb,
    0x7a172803, 0x7a29d6d3, 0x7a3c5d50, 0x7a4ebb9f, 0x7a60f1e6, 0x7a73004a,
    0x7a84e6f2, 0x7a96a604, 0x7aa83da7, 0x7ab9ae01, 0x7acaf73a, 0x7adc1979,
    0x7aed14e6, 0x7afde9a8, 0x7b0e97e8, 0x7b1f1fcd, 0x7b2f8182, 0x7b3fbd2d,
    0x7b4fd2f9, 0x7b5fc30f, 0x7b6f8d98, 0x7b7f32bd, 0x7b8eb2a9, 0x7b9e0d85,
    0x7bad437d, 0x7bbc54b9, 0x7bcb4166, 0x7bda09ae, 0x7be8adbc, 0x7bf72dbc,
    0x7c0589d8, 0x7c13c23d, 0x7c21d716, 0x7c2fc88f, 0x7c3d96d5, 0x7c4b4214,
    0x7c58ca78, 0x7c66302d, 0x7c737362, 0x7c809443, 0x7c8d92fc, 0x7c9a6fbc,
    0x7ca72aaf, 0x7cb3c404, 0x7cc03be8, 0x7ccc9288, 0x7cd8c814, 0x7ce4dcb9,
    0x7cf0d0a5, 0x7cfca406, 0x7d08570c, 0x7d13e9e5, 0x7d1f5cbf, 0x7d2aafca,
    0x7d35e335, 0x7d40f72e, 0x7d4bebe4, 0x7d56c188, 0x7d617848, 0x7d6c1054,
    0x7d7689db, 0x7d80e50e, 0x7d8b221b, 0x7d954133, 0x7d9f4286, 0x7da92643,
    0x7db2ec9b, 0x7dbc95bd, 0x7dc621da, 0x7dcf9123, 0x7dd8e3c6, 0x7de219f6,
    0x7deb33e2, 0x7df431ba, 0x7dfd13af, 0x7e05d9f2, 0x7e0e84b4, 0x7e171424,
    0x7e1f8874, 0x7e27e1d4, 0x7e302074, 0x7e384487, 0x7e404e3c, 0x7e483dc4,
    0x7e501350, 0x7e57cf11, 0x7e5f7138, 0x7e66f9f4, 0x7e6e6979, 0x7e75bff5,
    0x7e7cfd9a, 0x7e842298, 0x7e8b2f22, 0x7e922366, 0x7e98ff97, 0x7e9fc3e4,
    0x7ea6707f, 0x7ead0598, 0x7eb38360, 0x7eb9ea07, 0x7ec039bf, 0x7ec672b7,
    0x7ecc9521, 0x7ed2a12c, 0x7ed8970a, 0x7ede76ea, 0x7ee440fd, 0x7ee9f573,
    0x7eef947d, 0x7ef51e4b, 0x7efa930d, 0x7efff2f2, 0x7f053e2b, 0x7f0a74e8,
    0x7f0f9758, 0x7f14a5ac, 0x7f19a013, 0x7f1e86bc, 0x7f2359d8, 0x7f281995,
    0x7f2cc623, 0x7f315fb1, 0x7f35e66e, 0x7f3a5a8a, 0x7f3ebc33, 0x7f430b98,
    0x7f4748e7, 0x7f4b7450, 0x7f4f8e01, 0x7f539629, 0x7f578cf5, 0x7f5b7293,
    0x7f5f4732, 0x7f630b00, 0x7f66be2b, 0x7f6a60df, 0x7f6df34b, 0x7f71759b,
    0x7f74e7fe, 0x7f784aa0, 0x7f7b9daf, 0x7f7ee156, 0x7f8215c3, 0x7f853b22,
    0x7f88519f, 0x7f8b5967, 0x7f8e52a6, 0x7f913d87, 0x7f941a36, 0x7f96e8df,
    0x7f99a9ad, 0x7f9c5ccb, 0x7f9f0265, 0x7fa19aa5, 0x7fa425b5, 0x7fa6a3c1,
    0x7fa914f3, 0x7fab7974, 0x7fadd16f, 0x7fb01d0d, 0x7fb25c78, 0x7fb48fd9,
    0x7fb6b75a, 0x7fb8d323, 0x7fbae35d, 0x7fbce831, 0x7fbee1c7, 0x7fc0d047,
    0x7fc2b3d9, 0x7fc48ca5, 0x7fc65ad3, 0x7fc81e88, 0x7fc9d7ee, 0x7fcb872a,
    0x7fcd2c63, 0x7fcec7bf, 0x7fd05966, 0x7fd1e17c, 0x7fd36027, 0x7fd4d58d,
    0x7fd641d3, 0x7fd7a51e, 0x7fd8ff94, 0x7fda5157, 0x7fdb9a8e, 0x7fdcdb5b,
    0x7fde13e2, 0x7fdf4448, 0x7fe06caf, 0x7fe18d3b, 0x7fe2a60e, 0x7fe3b74b,
    0x7fe4c114, 0x7fe5c38b, 0x7fe6bed2, 0x7fe7b30a, 0x7fe8a055, 0x7fe986d4,
    0x7fea66a7, 0x7feb3ff0, 0x7fec12cd, 0x7fecdf5f, 0x7feda5c5, 0x7fee6620,
    0x7fef208d, 0x7fefd52c, 0x7ff0841c, 0x7ff12d7a, 0x7ff1d164, 0x7ff26ff9,
    0x7ff30955, 0x7ff39d96, 0x7ff42cd9, 0x7ff4b739, 0x7ff53cd4, 0x7ff5bdc5,
    0x7ff63a28, 0x7ff6b217, 0x7ff725af, 0x7ff7950a, 0x7ff80043, 0x7ff86773,
    0x7ff8cab4, 0x7ff92a21, 0x7ff985d1, 0x7ff9dddf, 0x7ffa3262, 0x7ffa8374,
    0x7ffad12c, 0x7ffb1ba1, 0x7ffb62ec, 0x7ffba723, 0x7ffbe85c, 0x7ffc26b0,
    0x7ffc6233, 0x7ffc9afb, 0x7ffcd11e, 0x7ffd04b1, 0x7ffd35c9, 0x7ffd647b,
    0x7ffd90da, 0x7ffdbafa, 0x7ffde2f0, 0x7ffe08ce, 0x7ffe2ca7, 0x7ffe4e8e,
    0x7ffe6e95, 0x7ffe8cce, 0x7ffea94a, 0x7ffec41b, 0x7ffedd52, 0x7ffef4ff,
    0x7fff0b33, 0x7fff1ffd, 0x7fff336e, 0x7fff4593, 0x7fff567d, 0x7fff663a,
    0x7fff74d8, 0x7fff8265, 0x7fff8eee, 0x7fff9a81, 0x7fffa52b, 0x7fffaef8,
    0x7fffb7f5, 0x7fffc02d, 0x7fffc7ab, 0x7fffce7c, 0x7fffd4a9, 0x7fffda3e,
    0x7fffdf44, 0x7fffe3c6, 0x7fffe7cc, 0x7fffeb60, 0x7fffee8a, 0x7ffff153,
    0x7ffff3c4, 0x7ffff5e3, 0x7ffff7b8, 0x7ffff94b, 0x7ffffaa1, 0x7ffffbc1,
    0x7ffffcb2, 0x7ffffd78, 0x7ffffe19, 0x7ffffe9a, 0x7ffffeff, 0x7fffff4e,
    0x7fffff89, 0x7fffffb3, 0x7fffffd2, 0x7fffffe6, 0x7ffffff3, 0x7ffffffa,
    0x7ffffffe, 0x7fffffff, 0x7fffffff, 0x7fffffff,
};

const  int32_t DecoderVorbisNative::vwin4096[2048] = {
    0x000001f0, 0x00001171, 0x00003072, 0x00005ef5, 0x00009cf8, 0x0000ea7c,
    0x00014780, 0x0001b405, 0x0002300b, 0x0002bb91, 0x00035698, 0x0004011e,
    0x0004bb25, 0x000584ac, 0x00065db3, 0x0007463a, 0x00083e41, 0x000945c7,
    0x000a5ccc, 0x000b8350, 0x000cb954, 0x000dfed7, 0x000f53d8, 0x0010b857,
    0x00122c55, 0x0013afd1, 0x001542ca, 0x0016e541, 0x00189735, 0x001a58a7,
    0x001c2995, 0x001e09ff, 0x001ff9e6, 0x0021f948, 0x00240826, 0x00262680,
    0x00285454, 0x002a91a3, 0x002cde6c, 0x002f3aaf, 0x0031a66b, 0x003421a0,
    0x0036ac4f, 0x00394675, 0x003bf014, 0x003ea92a, 0x004171b7, 0x004449bb,
    0x00473135, 0x004a2824, 0x004d2e8a, 0x00504463, 0x005369b2, 0x00569e74,
    0x0059e2aa, 0x005d3652, 0x0060996d, 0x00640bf9, 0x00678df7, 0x006b1f66,
    0x006ec045, 0x00727093, 0x00763051, 0x0079ff7d, 0x007dde16, 0x0081cc1d,
    0x0085c991, 0x0089d671, 0x008df2bc, 0x00921e71, 0x00965991, 0x009aa41a,
    0x009efe0c, 0x00a36766, 0x00a7e028, 0x00ac6850, 0x00b0ffde, 0x00b5a6d1,
    0x00ba5d28, 0x00bf22e4, 0x00c3f802, 0x00c8dc83, 0x00cdd065, 0x00d2d3a8,
    0x00d7e64a, 0x00dd084c, 0x00e239ac, 0x00e77a69, 0x00ecca83, 0x00f229f9,
    0x00f798ca, 0x00fd16f5, 0x0102a479, 0x01084155, 0x010ded89, 0x0113a913,
    0x011973f3, 0x011f4e27, 0x012537af, 0x012b308a, 0x013138b7, 0x01375035,
    0x013d7702, 0x0143ad1f, 0x0149f289, 0x01504741, 0x0156ab44, 0x015d1e92,
    0x0163a12a, 0x016a330b, 0x0170d433, 0x017784a3, 0x017e4458, 0x01851351,
    0x018bf18e, 0x0192df0d, 0x0199dbcd, 0x01a0e7cd, 0x01a8030c, 0x01af2d89,
    0x01b66743, 0x01bdb038, 0x01c50867, 0x01cc6fd0, 0x01d3e670, 0x01db6c47,
    0x01e30153, 0x01eaa593, 0x01f25907, 0x01fa1bac, 0x0201ed81, 0x0209ce86,
    0x0211beb8, 0x0219be17, 0x0221cca2, 0x0229ea56, 0x02321733, 0x023a5337,
    0x02429e60, 0x024af8af, 0x02536220, 0x025bdab3, 0x02646267, 0x026cf93a,
    0x02759f2a, 0x027e5436, 0x0287185d, 0x028feb9d, 0x0298cdf4, 0x02a1bf62,
    0x02aabfe5, 0x02b3cf7b, 0x02bcee23, 0x02c61bdb, 0x02cf58a2, 0x02d8a475,
    0x02e1ff55, 0x02eb693e, 0x02f4e230, 0x02fe6a29, 0x03080127, 0x0311a729,
    0x031b5c2d, 0x03252031, 0x032ef334, 0x0338d534, 0x0342c630, 0x034cc625,
    0x0356d512, 0x0360f2f6, 0x036b1fce, 0x03755b99, 0x037fa655, 0x038a0001,
    0x0394689a, 0x039ee020, 0x03a9668f, 0x03b3fbe6, 0x03bea024, 0x03c95347,
    0x03d4154d, 0x03dee633, 0x03e9c5f9, 0x03f4b49b, 0x03ffb219, 0x040abe71,
    0x0415d9a0, 0x042103a5, 0x042c3c7d, 0x04378428, 0x0442daa2, 0x044e3fea,
    0x0459b3fd, 0x046536db, 0x0470c880, 0x047c68eb, 0x0488181a, 0x0493d60b,
    0x049fa2bc, 0x04ab7e2a, 0x04b76854, 0x04c36137, 0x04cf68d1, 0x04db7f21,
    0x04e7a424, 0x04f3d7d8, 0x05001a3b, 0x050c6b4a, 0x0518cb04, 0x05253966,
    0x0531b66e, 0x053e421a, 0x054adc68, 0x05578555, 0x05643cdf, 0x05710304,
    0x057dd7c1, 0x058abb15, 0x0597acfd, 0x05a4ad76, 0x05b1bc7f, 0x05beda14,
    0x05cc0635, 0x05d940dd, 0x05e68a0b, 0x05f3e1bd, 0x060147f0, 0x060ebca1,
    0x061c3fcf, 0x0629d176, 0x06377194, 0x06452027, 0x0652dd2c, 0x0660a8a2,
    0x066e8284, 0x067c6ad1, 0x068a6186, 0x069866a1, 0x06a67a1e, 0x06b49bfc,
    0x06c2cc38, 0x06d10acf, 0x06df57bf, 0x06edb304, 0x06fc1c9d, 0x070a9487,
    0x07191abe, 0x0727af40, 0x0736520b, 0x0745031c, 0x0753c270, 0x07629004,
    0x07716bd6, 0x078055e2, 0x078f4e26, 0x079e549f, 0x07ad694b, 0x07bc8c26,
    0x07cbbd2e, 0x07dafc5f, 0x07ea49b7, 0x07f9a533, 0x08090ed1, 0x0818868c,
    0x08280c62, 0x0837a051, 0x08474255, 0x0856f26b, 0x0866b091, 0x08767cc3,
    0x088656fe, 0x08963f3f, 0x08a63584, 0x08b639c8, 0x08c64c0a, 0x08d66c45,
    0x08e69a77, 0x08f6d69d, 0x090720b3, 0x091778b7, 0x0927dea5, 0x0938527a,
    0x0948d433, 0x095963cc, 0x096a0143, 0x097aac94, 0x098b65bb, 0x099c2cb6,
    0x09ad0182, 0x09bde41a, 0x09ced47d, 0x09dfd2a5, 0x09f0de90, 0x0a01f83b,
    0x0a131fa3, 0x0a2454c3, 0x0a359798, 0x0a46e820, 0x0a584656, 0x0a69b237,
    0x0a7b2bc0, 0x0a8cb2ec, 0x0a9e47ba, 0x0aafea24, 0x0ac19a29, 0x0ad357c3,
    0x0ae522ef, 0x0af6fbab, 0x0b08e1f1, 0x0b1ad5c0, 0x0b2cd712, 0x0b3ee5e5,
    0x0b510234, 0x0b632bfd, 0x0b75633b, 0x0b87a7eb, 0x0b99fa08, 0x0bac5990,
    0x0bbec67e, 0x0bd140cf, 0x0be3c87e, 0x0bf65d89, 0x0c08ffeb, 0x0c1bafa1,
    0x0c2e6ca6, 0x0c4136f6, 0x0c540e8f, 0x0c66f36c, 0x0c79e588, 0x0c8ce4e1,
    0x0c9ff172, 0x0cb30b37, 0x0cc6322c, 0x0cd9664d, 0x0ceca797, 0x0cfff605,
    0x0d135193, 0x0d26ba3d, 0x0d3a2fff, 0x0d4db2d5, 0x0d6142ba, 0x0d74dfac,
    0x0d8889a5, 0x0d9c40a1, 0x0db0049d, 0x0dc3d593, 0x0dd7b380, 0x0deb9e60,
    0x0dff962f, 0x0e139ae7, 0x0e27ac85, 0x0e3bcb05, 0x0e4ff662, 0x0e642e98,
    0x0e7873a2, 0x0e8cc57d, 0x0ea12423, 0x0eb58f91, 0x0eca07c2, 0x0ede8cb1,
    0x0ef31e5b, 0x0f07bcba, 0x0f1c67cb, 0x0f311f88, 0x0f45e3ee, 0x0f5ab4f7,
    0x0f6f92a0, 0x0f847ce3, 0x0f9973bc, 0x0fae7726, 0x0fc3871e, 0x0fd8a39d,
    0x0fedcca1, 0x10030223, 0x1018441f, 0x102d9291, 0x1042ed74, 0x105854c3,
    0x106dc879, 0x10834892, 0x1098d508, 0x10ae6dd8, 0x10c412fc, 0x10d9c46f,
    0x10ef822d, 0x11054c30, 0x111b2274, 0x113104f5, 0x1146f3ac, 0x115cee95,
    0x1172f5ab, 0x118908e9, 0x119f284a, 0x11b553ca, 0x11cb8b62, 0x11e1cf0f,
    0x11f81ecb, 0x120e7a90, 0x1224e25a, 0x123b5624, 0x1251d5e9, 0x126861a3,
    0x127ef94e, 0x12959ce3, 0x12ac4c5f, 0x12c307bb, 0x12d9cef2, 0x12f0a200,
    0x130780df, 0x131e6b8a, 0x133561fa, 0x134c642c, 0x1363721a, 0x137a8bbe,
    0x1391b113, 0x13a8e214, 0x13c01eba, 0x13d76702, 0x13eebae5, 0x14061a5e,
    0x141d8567, 0x1434fbfb, 0x144c7e14, 0x14640bae, 0x147ba4c1, 0x14934949,
    0x14aaf941, 0x14c2b4a2, 0x14da7b67, 0x14f24d8a, 0x150a2b06, 0x152213d5,
    0x153a07f1, 0x15520755, 0x156a11fb, 0x158227dd, 0x159a48f5, 0x15b2753d,
    0x15caacb1, 0x15e2ef49, 0x15fb3d01, 0x161395d2, 0x162bf9b6, 0x164468a8,
    0x165ce2a1, 0x1675679c, 0x168df793, 0x16a69280, 0x16bf385c, 0x16d7e922,
    0x16f0a4cc, 0x17096b54, 0x17223cb4, 0x173b18e5, 0x1753ffe2, 0x176cf1a5,
    0x1785ee27, 0x179ef562, 0x17b80750, 0x17d123eb, 0x17ea4b2d, 0x18037d10,
    0x181cb98d, 0x1836009e, 0x184f523c, 0x1868ae63, 0x1882150a, 0x189b862c,
    0x18b501c4, 0x18ce87c9, 0x18e81836, 0x1901b305, 0x191b582f, 0x193507ad,
    0x194ec17a, 0x1968858f, 0x198253e5, 0x199c2c75, 0x19b60f3a, 0x19cffc2d,
    0x19e9f347, 0x1a03f482, 0x1a1dffd7, 0x1a381540, 0x1a5234b5, 0x1a6c5e31,
    0x1a8691ac, 0x1aa0cf21, 0x1abb1687, 0x1ad567da, 0x1aefc311, 0x1b0a2826,
    0x1b249712, 0x1b3f0fd0, 0x1b599257, 0x1b741ea1, 0x1b8eb4a7, 0x1ba95462,
    0x1bc3fdcd, 0x1bdeb0de, 0x1bf96d91, 0x1c1433dd, 0x1c2f03bc, 0x1c49dd27,
    0x1c64c017, 0x1c7fac85, 0x1c9aa269, 0x1cb5a1be, 0x1cd0aa7c, 0x1cebbc9c,
    0x1d06d816, 0x1d21fce4, 0x1d3d2aff, 0x1d586260, 0x1d73a2fe, 0x1d8eecd4,
    0x1daa3fda, 0x1dc59c09, 0x1de1015a, 0x1dfc6fc5, 0x1e17e743, 0x1e3367cd,
    0x1e4ef15b, 0x1e6a83e7, 0x1e861f6a, 0x1ea1c3da, 0x1ebd7133, 0x1ed9276b,
    0x1ef4e67c, 0x1f10ae5e, 0x1f2c7f0a, 0x1f485879, 0x1f643aa2, 0x1f80257f,
    0x1f9c1908, 0x1fb81536, 0x1fd41a00, 0x1ff02761, 0x200c3d4f, 0x20285bc3,
    0x204482b7, 0x2060b221, 0x207ce9fb, 0x20992a3e, 0x20b572e0, 0x20d1c3dc,
    0x20ee1d28, 0x210a7ebe, 0x2126e895, 0x21435aa6, 0x215fd4ea, 0x217c5757,
    0x2198e1e8, 0x21b57493, 0x21d20f51, 0x21eeb21b, 0x220b5ce7, 0x22280fb0,
    0x2244ca6c, 0x22618d13, 0x227e579f, 0x229b2a06, 0x22b80442, 0x22d4e649,
    0x22f1d015, 0x230ec19d, 0x232bbad9, 0x2348bbc1, 0x2365c44c, 0x2382d474,
    0x239fec30, 0x23bd0b78, 0x23da3244, 0x23f7608b, 0x24149646, 0x2431d36c,
    0x244f17f5, 0x246c63da, 0x2489b711, 0x24a71193, 0x24c47358, 0x24e1dc57,
    0x24ff4c88, 0x251cc3e2, 0x253a425e, 0x2557c7f4, 0x2575549a, 0x2592e848,
    0x25b082f7, 0x25ce249e, 0x25ebcd34, 0x26097cb2, 0x2627330e, 0x2644f040,
    0x2662b441, 0x26807f07, 0x269e5089, 0x26bc28c1, 0x26da07a4, 0x26f7ed2b,
    0x2715d94d, 0x2733cc02, 0x2751c540, 0x276fc500, 0x278dcb39, 0x27abd7e2,
    0x27c9eaf3, 0x27e80463, 0x28062429, 0x28244a3e, 0x28427697, 0x2860a92d,
    0x287ee1f7, 0x289d20eb, 0x28bb6603, 0x28d9b134, 0x28f80275, 0x291659c0,
    0x2934b709, 0x29531a49, 0x29718378, 0x298ff28b, 0x29ae677b, 0x29cce23e,
    0x29eb62cb, 0x2a09e91b, 0x2a287523, 0x2a4706dc, 0x2a659e3c, 0x2a843b39,
    0x2aa2ddcd, 0x2ac185ec, 0x2ae0338f, 0x2afee6ad, 0x2b1d9f3c, 0x2b3c5d33,
    0x2b5b208b, 0x2b79e939, 0x2b98b734, 0x2bb78a74, 0x2bd662ef, 0x2bf5409d,
    0x2c142374, 0x2c330b6b, 0x2c51f87a, 0x2c70ea97, 0x2c8fe1b9, 0x2caeddd6,
    0x2ccddee7, 0x2cece4e1, 0x2d0befbb, 0x2d2aff6d, 0x2d4a13ec, 0x2d692d31,
    0x2d884b32, 0x2da76de4, 0x2dc69540, 0x2de5c13d, 0x2e04f1d0, 0x2e2426f0,
    0x2e436095, 0x2e629eb4, 0x2e81e146, 0x2ea1283f, 0x2ec07398, 0x2edfc347,
    0x2eff1742, 0x2f1e6f80, 0x2f3dcbf8, 0x2f5d2ca0, 0x2f7c916f, 0x2f9bfa5c,
    0x2fbb675d, 0x2fdad869, 0x2ffa4d76, 0x3019c67b, 0x3039436f, 0x3058c448,
    0x307848fc, 0x3097d183, 0x30b75dd3, 0x30d6ede2, 0x30f681a6, 0x31161917,
    0x3135b42b, 0x315552d8, 0x3174f514, 0x31949ad7, 0x31b44417, 0x31d3f0ca,
    0x31f3a0e6, 0x32135462, 0x32330b35, 0x3252c555, 0x327282b7, 0x32924354,
    0x32b20720, 0x32d1ce13, 0x32f19823, 0x33116546, 0x33313573, 0x3351089f,
    0x3370dec2, 0x3390b7d1, 0x33b093c3, 0x33d0728f, 0x33f05429, 0x3410388a,
    0x34301fa7, 0x34500977, 0x346ff5ef, 0x348fe506, 0x34afd6b3, 0x34cfcaeb,
    0x34efc1a5, 0x350fbad7, 0x352fb678, 0x354fb47d, 0x356fb4dd, 0x358fb78e,
    0x35afbc86, 0x35cfc3bc, 0x35efcd25, 0x360fd8b8, 0x362fe66c, 0x364ff636,
    0x3670080c, 0x36901be5, 0x36b031b7, 0x36d04978, 0x36f0631e, 0x37107ea0,
    0x37309bf3, 0x3750bb0e, 0x3770dbe6, 0x3790fe73, 0x37b122aa, 0x37d14881,
    0x37f16fee, 0x381198e8, 0x3831c365, 0x3851ef5a, 0x38721cbe, 0x38924b87,
    0x38b27bac, 0x38d2ad21, 0x38f2dfde, 0x391313d8, 0x39334906, 0x39537f5d,
    0x3973b6d4, 0x3993ef60, 0x39b428f9, 0x39d46393, 0x39f49f25, 0x3a14dba6,
    0x3a35190a, 0x3a555748, 0x3a759657, 0x3a95d62c, 0x3ab616be, 0x3ad65801,
    0x3af699ed, 0x3b16dc78, 0x3b371f97, 0x3b576341, 0x3b77a76c, 0x3b97ec0d,
    0x3bb8311b, 0x3bd8768b, 0x3bf8bc55, 0x3c19026d, 0x3c3948ca, 0x3c598f62,
    0x3c79d62b, 0x3c9a1d1b, 0x3cba6428, 0x3cdaab48, 0x3cfaf271, 0x3d1b3999,
    0x3d3b80b6, 0x3d5bc7be, 0x3d7c0ea8, 0x3d9c5569, 0x3dbc9bf7, 0x3ddce248,
    0x3dfd2852, 0x3e1d6e0c, 0x3e3db36c, 0x3e5df866, 0x3e7e3cf2, 0x3e9e8106,
    0x3ebec497, 0x3edf079b, 0x3eff4a09, 0x3f1f8bd7, 0x3f3fccfa, 0x3f600d69,
    0x3f804d1a, 0x3fa08c02, 0x3fc0ca19, 0x3fe10753, 0x400143a7, 0x40217f0a,
    0x4041b974, 0x4061f2da, 0x40822b32, 0x40a26272, 0x40c29891, 0x40e2cd83,
    0x41030140, 0x412333bd, 0x414364f1, 0x416394d2, 0x4183c355, 0x41a3f070,
    0x41c41c1b, 0x41e4464a, 0x42046ef4, 0x42249610, 0x4244bb92, 0x4264df72,
    0x428501a5, 0x42a52222, 0x42c540de, 0x42e55dd0, 0x430578ed, 0x4325922d,
    0x4345a985, 0x4365beeb, 0x4385d255, 0x43a5e3ba, 0x43c5f30f, 0x43e6004b,
    0x44060b65, 0x44261451, 0x44461b07, 0x44661f7c, 0x448621a7, 0x44a6217d,
    0x44c61ef6, 0x44e61a07, 0x450612a6, 0x452608ca, 0x4545fc69, 0x4565ed79,
    0x4585dbf1, 0x45a5c7c6, 0x45c5b0ef, 0x45e59761, 0x46057b15, 0x46255bfe,
    0x46453a15, 0x4665154f, 0x4684eda2, 0x46a4c305, 0x46c4956e, 0x46e464d3,
    0x4704312b, 0x4723fa6c, 0x4743c08d, 0x47638382, 0x47834344, 0x47a2ffc9,
    0x47c2b906, 0x47e26ef2, 0x48022183, 0x4821d0b1, 0x48417c71, 0x486124b9,
    0x4880c981, 0x48a06abe, 0x48c00867, 0x48dfa272, 0x48ff38d6, 0x491ecb8a,
    0x493e5a84, 0x495de5b9, 0x497d6d22, 0x499cf0b4, 0x49bc7066, 0x49dbec2e,
    0x49fb6402, 0x4a1ad7db, 0x4a3a47ad, 0x4a59b370, 0x4a791b1a, 0x4a987ea1,
    0x4ab7ddfd, 0x4ad73924, 0x4af6900c, 0x4b15e2ad, 0x4b3530fc, 0x4b547af1,
    0x4b73c082, 0x4b9301a6, 0x4bb23e53, 0x4bd17681, 0x4bf0aa25, 0x4c0fd937,
    0x4c2f03ae, 0x4c4e297f, 0x4c6d4aa3, 0x4c8c670f, 0x4cab7eba, 0x4cca919c,
    0x4ce99fab, 0x4d08a8de, 0x4d27ad2c, 0x4d46ac8b, 0x4d65a6f3, 0x4d849c5a,
    0x4da38cb7, 0x4dc27802, 0x4de15e31, 0x4e003f3a, 0x4e1f1b16, 0x4e3df1ba,
    0x4e5cc31e, 0x4e7b8f3a, 0x4e9a5603, 0x4eb91771, 0x4ed7d37b, 0x4ef68a18,
    0x4f153b3f, 0x4f33e6e7, 0x4f528d08, 0x4f712d97, 0x4f8fc88e, 0x4fae5de1,
    0x4fcced8a, 0x4feb777f, 0x5009fbb6, 0x50287a28, 0x5046f2cc, 0x50656598,
    0x5083d284, 0x50a23988, 0x50c09a9a, 0x50def5b1, 0x50fd4ac7, 0x511b99d0,
    0x5139e2c5, 0x5158259e, 0x51766251, 0x519498d6, 0x51b2c925, 0x51d0f334,
    0x51ef16fb, 0x520d3473, 0x522b4b91, 0x52495c4e, 0x526766a2, 0x52856a83,
    0x52a367e9, 0x52c15ecd, 0x52df4f24, 0x52fd38e8, 0x531b1c10, 0x5338f892,
    0x5356ce68, 0x53749d89, 0x539265eb, 0x53b02788, 0x53cde257, 0x53eb964f,
    0x54094369, 0x5426e99c, 0x544488df, 0x5462212c, 0x547fb279, 0x549d3cbe,
    0x54babff4, 0x54d83c12, 0x54f5b110, 0x55131ee7, 0x5530858d, 0x554de4fc,
    0x556b3d2a, 0x55888e11, 0x55a5d7a8, 0x55c319e7, 0x55e054c7, 0x55fd883f,
    0x561ab447, 0x5637d8d8, 0x5654f5ea, 0x56720b75, 0x568f1971, 0x56ac1fd7,
    0x56c91e9e, 0x56e615c0, 0x57030534, 0x571fecf2, 0x573cccf3, 0x5759a530,
    0x577675a0, 0x57933e3c, 0x57affefd, 0x57ccb7db, 0x57e968ce, 0x580611cf,
    0x5822b2d6, 0x583f4bdd, 0x585bdcdb, 0x587865c9, 0x5894e69f, 0x58b15f57,
    0x58cdcfe9, 0x58ea384e, 0x5906987d, 0x5922f071, 0x593f4022, 0x595b8788,
    0x5977c69c, 0x5993fd57, 0x59b02bb2, 0x59cc51a6, 0x59e86f2c, 0x5a04843c,
    0x5a2090d0, 0x5a3c94e0, 0x5a589065, 0x5a748359, 0x5a906db4, 0x5aac4f70,
    0x5ac82884, 0x5ae3f8ec, 0x5affc09f, 0x5b1b7f97, 0x5b3735cd, 0x5b52e33a,
    0x5b6e87d8, 0x5b8a239f, 0x5ba5b689, 0x5bc1408f, 0x5bdcc1aa, 0x5bf839d5,
    0x5c13a907, 0x5c2f0f3b, 0x5c4a6c6a, 0x5c65c08d, 0x5c810b9e, 0x5c9c4d97,
    0x5cb78670, 0x5cd2b623, 0x5ceddcaa, 0x5d08f9ff, 0x5d240e1b, 0x5d3f18f8,
    0x5d5a1a8f, 0x5d7512da, 0x5d9001d3, 0x5daae773, 0x5dc5c3b5, 0x5de09692,
    0x5dfb6004, 0x5e162004, 0x5e30d68d, 0x5e4b8399, 0x5e662721, 0x5e80c11f,
    0x5e9b518e, 0x5eb5d867, 0x5ed055a4, 0x5eeac940, 0x5f053334, 0x5f1f937b,
    0x5f39ea0f, 0x5f5436ea, 0x5f6e7a06, 0x5f88b35d, 0x5fa2e2e9, 0x5fbd08a6,
    0x5fd7248d, 0x5ff13698, 0x600b3ec2, 0x60253d05, 0x603f315b, 0x60591bc0,
    0x6072fc2d, 0x608cd29e, 0x60a69f0b, 0x60c06171, 0x60da19ca, 0x60f3c80f,
    0x610d6c3d, 0x6127064d, 0x6140963a, 0x615a1bff, 0x61739797, 0x618d08fc,
    0x61a67029, 0x61bfcd1a, 0x61d91fc8, 0x61f2682f, 0x620ba64a, 0x6224da13,
    0x623e0386, 0x6257229d, 0x62703754, 0x628941a6, 0x62a2418e, 0x62bb3706,
    0x62d4220a, 0x62ed0296, 0x6305d8a3, 0x631ea42f, 0x63376533, 0x63501bab,
    0x6368c793, 0x638168e5, 0x6399ff9e, 0x63b28bb8, 0x63cb0d2f, 0x63e383ff,
    0x63fbf022, 0x64145195, 0x642ca853, 0x6444f457, 0x645d359e, 0x64756c22,
    0x648d97e0, 0x64a5b8d3, 0x64bdcef6, 0x64d5da47, 0x64eddabf, 0x6505d05c,
    0x651dbb19, 0x65359af2, 0x654d6fe3, 0x656539e7, 0x657cf8fb, 0x6594ad1b,
    0x65ac5643, 0x65c3f46e, 0x65db8799, 0x65f30fc0, 0x660a8ce0, 0x6621fef3,
    0x663965f7, 0x6650c1e7, 0x666812c1, 0x667f5880, 0x66969320, 0x66adc29e,
    0x66c4e6f7, 0x66dc0026, 0x66f30e28, 0x670a10fa, 0x67210898, 0x6737f4ff,
    0x674ed62b, 0x6765ac19, 0x677c76c5, 0x6793362c, 0x67a9ea4b, 0x67c0931f,
    0x67d730a3, 0x67edc2d6, 0x680449b3, 0x681ac538, 0x68313562, 0x68479a2d,
    0x685df396, 0x6874419b, 0x688a8438, 0x68a0bb6a, 0x68b6e72e, 0x68cd0782,
    0x68e31c63, 0x68f925cd, 0x690f23be, 0x69251633, 0x693afd29, 0x6950d89e,
    0x6966a88f, 0x697c6cf8, 0x699225d9, 0x69a7d32d, 0x69bd74f3, 0x69d30b27,
    0x69e895c8, 0x69fe14d2, 0x6a138844, 0x6a28f01b, 0x6a3e4c54, 0x6a539ced,
    0x6a68e1e4, 0x6a7e1b37, 0x6a9348e3, 0x6aa86ae6, 0x6abd813d, 0x6ad28be7,
    0x6ae78ae2, 0x6afc7e2b, 0x6b1165c0, 0x6b26419f, 0x6b3b11c7, 0x6b4fd634,
    0x6b648ee6, 0x6b793bda, 0x6b8ddd0e, 0x6ba27281, 0x6bb6fc31, 0x6bcb7a1b,
    0x6bdfec3e, 0x6bf45299, 0x6c08ad29, 0x6c1cfbed, 0x6c313ee4, 0x6c45760a,
    0x6c59a160, 0x6c6dc0e4, 0x6c81d493, 0x6c95dc6d, 0x6ca9d86f, 0x6cbdc899,
    0x6cd1acea, 0x6ce5855f, 0x6cf951f7, 0x6d0d12b1, 0x6d20c78c, 0x6d347087,
    0x6d480da0, 0x6d5b9ed6, 0x6d6f2427, 0x6d829d94, 0x6d960b1a, 0x6da96cb9,
    0x6dbcc270, 0x6dd00c3c, 0x6de34a1f, 0x6df67c16, 0x6e09a221, 0x6e1cbc3f,
    0x6e2fca6e, 0x6e42ccaf, 0x6e55c300, 0x6e68ad60, 0x6e7b8bd0, 0x6e8e5e4d,
    0x6ea124d8, 0x6eb3df70, 0x6ec68e13, 0x6ed930c3, 0x6eebc77d, 0x6efe5242,
    0x6f10d111, 0x6f2343e9, 0x6f35aacb, 0x6f4805b5, 0x6f5a54a8, 0x6f6c97a2,
    0x6f7ecea4, 0x6f90f9ae, 0x6fa318be, 0x6fb52bd6, 0x6fc732f4, 0x6fd92e19,
    0x6feb1d44, 0x6ffd0076, 0x700ed7ad, 0x7020a2eb, 0x7032622f, 0x7044157a,
    0x7055bcca, 0x70675821, 0x7078e77e, 0x708a6ae2, 0x709be24c, 0x70ad4dbd,
    0x70bead36, 0x70d000b5, 0x70e1483d, 0x70f283cc, 0x7103b363, 0x7114d704,
    0x7125eead, 0x7136fa60, 0x7147fa1c, 0x7158ede4, 0x7169d5b6, 0x717ab193,
    0x718b817d, 0x719c4573, 0x71acfd76, 0x71bda988, 0x71ce49a8, 0x71deddd7,
    0x71ef6617, 0x71ffe267, 0x721052ca, 0x7220b73e, 0x72310fc6, 0x72415c62,
    0x72519d14, 0x7261d1db, 0x7271faba, 0x728217b1, 0x729228c0, 0x72a22dea,
    0x72b22730, 0x72c21491, 0x72d1f611, 0x72e1cbaf, 0x72f1956c, 0x7301534c,
    0x7311054d, 0x7320ab72, 0x733045bc, 0x733fd42d, 0x734f56c5, 0x735ecd86,
    0x736e3872, 0x737d9789, 0x738ceacf, 0x739c3243, 0x73ab6de7, 0x73ba9dbe,
    0x73c9c1c8, 0x73d8da08, 0x73e7e67f, 0x73f6e72e, 0x7405dc17, 0x7414c53c,
    0x7423a29f, 0x74327442, 0x74413a26, 0x744ff44d, 0x745ea2b9, 0x746d456c,
    0x747bdc68, 0x748a67ae, 0x7498e741, 0x74a75b23, 0x74b5c356, 0x74c41fdb,
    0x74d270b6, 0x74e0b5e7, 0x74eeef71, 0x74fd1d57, 0x750b3f9a, 0x7519563c,
    0x75276140, 0x753560a8, 0x75435477, 0x75513cae, 0x755f1951, 0x756cea60,
    0x757aafdf, 0x758869d1, 0x75961837, 0x75a3bb14, 0x75b1526a, 0x75bede3c,
    0x75cc5e8d, 0x75d9d35f, 0x75e73cb5, 0x75f49a91, 0x7601ecf6, 0x760f33e6,
    0x761c6f65, 0x76299f74, 0x7636c417, 0x7643dd51, 0x7650eb24, 0x765ded93,
    0x766ae4a0, 0x7677d050, 0x7684b0a4, 0x7691859f, 0x769e4f45, 0x76ab0d98,
    0x76b7c09c, 0x76c46852, 0x76d104bf, 0x76dd95e6, 0x76ea1bc9, 0x76f6966b,
    0x770305d0, 0x770f69fb, 0x771bc2ef, 0x772810af, 0x7734533e, 0x77408aa0,
    0x774cb6d7, 0x7758d7e8, 0x7764edd5, 0x7770f8a2, 0x777cf852, 0x7788ece8,
    0x7794d668, 0x77a0b4d5, 0x77ac8833, 0x77b85085, 0x77c40dce, 0x77cfc013,
    0x77db6756, 0x77e7039b, 0x77f294e6, 0x77fe1b3b, 0x7809969c, 0x7815070e,
    0x78206c93, 0x782bc731, 0x783716ea, 0x78425bc3, 0x784d95be, 0x7858c4e1,
    0x7863e92d, 0x786f02a8, 0x787a1156, 0x78851539, 0x78900e56, 0x789afcb1,
    0x78a5e04d, 0x78b0b92f, 0x78bb875b, 0x78c64ad4, 0x78d1039e, 0x78dbb1be,
    0x78e65537, 0x78f0ee0e, 0x78fb7c46, 0x7905ffe4, 0x791078ec, 0x791ae762,
    0x79254b4a, 0x792fa4a7, 0x7939f380, 0x794437d7, 0x794e71b0, 0x7958a111,
    0x7962c5fd, 0x796ce078, 0x7976f087, 0x7980f62f, 0x798af173, 0x7994e258,
    0x799ec8e2, 0x79a8a515, 0x79b276f7, 0x79bc3e8b, 0x79c5fbd6, 0x79cfaedc,
    0x79d957a2, 0x79e2f62c, 0x79ec8a7f, 0x79f6149f, 0x79ff9492, 0x7a090a5a,
    0x7a1275fe, 0x7a1bd781, 0x7a252ee9, 0x7a2e7c39, 0x7a37bf77, 0x7a40f8a7,
    0x7a4a27ce, 0x7a534cf0, 0x7a5c6813, 0x7a65793b, 0x7a6e806d, 0x7a777dad,
    0x7a807100, 0x7a895a6b, 0x7a9239f4, 0x7a9b0f9e, 0x7aa3db6f, 0x7aac9d6b,
    0x7ab55597, 0x7abe03f9, 0x7ac6a895, 0x7acf4370, 0x7ad7d48f, 0x7ae05bf6,
    0x7ae8d9ac, 0x7af14db5, 0x7af9b815, 0x7b0218d2, 0x7b0a6ff2, 0x7b12bd78,
    0x7b1b016a, 0x7b233bce, 0x7b2b6ca7, 0x7b3393fc, 0x7b3bb1d1, 0x7b43c62c,
    0x7b4bd111, 0x7b53d286, 0x7b5bca90, 0x7b63b935, 0x7b6b9e78, 0x7b737a61,
    0x7b7b4cf3, 0x7b831634, 0x7b8ad629, 0x7b928cd8, 0x7b9a3a45, 0x7ba1de77,
    0x7ba97972, 0x7bb10b3c, 0x7bb893d9, 0x7bc01350, 0x7bc789a6, 0x7bcef6e0,
    0x7bd65b03, 0x7bddb616, 0x7be5081c, 0x7bec511c, 0x7bf3911b, 0x7bfac81f,
    0x7c01f62c, 0x7c091b49, 0x7c10377b, 0x7c174ac7, 0x7c1e5532, 0x7c2556c4,
    0x7c2c4f80, 0x7c333f6c, 0x7c3a268e, 0x7c4104ec, 0x7c47da8a, 0x7c4ea76f,
    0x7c556ba1, 0x7c5c2724, 0x7c62d9fe, 0x7c698435, 0x7c7025cf, 0x7c76bed0,
    0x7c7d4f40, 0x7c83d723, 0x7c8a567f, 0x7c90cd5a, 0x7c973bb9, 0x7c9da1a2,
    0x7ca3ff1b, 0x7caa542a, 0x7cb0a0d3, 0x7cb6e51e, 0x7cbd210f, 0x7cc354ac,
    0x7cc97ffc, 0x7ccfa304, 0x7cd5bdc9, 0x7cdbd051, 0x7ce1daa3, 0x7ce7dcc3,
    0x7cedd6b8, 0x7cf3c888, 0x7cf9b238, 0x7cff93cf, 0x7d056d51, 0x7d0b3ec5,
    0x7d110830, 0x7d16c99a, 0x7d1c8306, 0x7d22347c, 0x7d27de00, 0x7d2d7f9a,
    0x7d33194f, 0x7d38ab24, 0x7d3e351f, 0x7d43b748, 0x7d4931a2, 0x7d4ea435,
    0x7d540f06, 0x7d59721b, 0x7d5ecd7b, 0x7d64212a, 0x7d696d2f, 0x7d6eb190,
    0x7d73ee53, 0x7d79237e, 0x7d7e5117, 0x7d837723, 0x7d8895a9, 0x7d8dacae,
    0x7d92bc3a, 0x7d97c451, 0x7d9cc4f9, 0x7da1be39, 0x7da6b017, 0x7dab9a99,
    0x7db07dc4, 0x7db5599e, 0x7dba2e2f, 0x7dbefb7b, 0x7dc3c189, 0x7dc8805e,
    0x7dcd3802, 0x7dd1e879, 0x7dd691ca, 0x7ddb33fb, 0x7ddfcf12, 0x7de46315,
    0x7de8f00a, 0x7ded75f8, 0x7df1f4e3, 0x7df66cd3, 0x7dfaddcd, 0x7dff47d7,
    0x7e03aaf8, 0x7e080735, 0x7e0c5c95, 0x7e10ab1e, 0x7e14f2d5, 0x7e1933c1,
    0x7e1d6de8, 0x7e21a150, 0x7e25cdff, 0x7e29f3fc, 0x7e2e134c, 0x7e322bf5,
    0x7e363dfd, 0x7e3a496b, 0x7e3e4e45, 0x7e424c90, 0x7e464454, 0x7e4a3595,
    0x7e4e205a, 0x7e5204aa, 0x7e55e289, 0x7e59b9ff, 0x7e5d8b12, 0x7e6155c7,
    0x7e651a24, 0x7e68d831, 0x7e6c8ff2, 0x7e70416e, 0x7e73ecac, 0x7e7791b0,
    0x7e7b3082, 0x7e7ec927, 0x7e825ba6, 0x7e85e804, 0x7e896e48, 0x7e8cee77,
    0x7e906899, 0x7e93dcb2, 0x7e974aca, 0x7e9ab2e5, 0x7e9e150b, 0x7ea17141,
    0x7ea4c78e, 0x7ea817f7, 0x7eab6283, 0x7eaea737, 0x7eb1e61a, 0x7eb51f33,
    0x7eb85285, 0x7ebb8019, 0x7ebea7f4, 0x7ec1ca1d, 0x7ec4e698, 0x7ec7fd6d,
    0x7ecb0ea1, 0x7ece1a3a, 0x7ed1203f, 0x7ed420b6, 0x7ed71ba4, 0x7eda110f,
    0x7edd00ff, 0x7edfeb78, 0x7ee2d081, 0x7ee5b01f, 0x7ee88a5a, 0x7eeb5f36,
    0x7eee2eba, 0x7ef0f8ed, 0x7ef3bdd3, 0x7ef67d73, 0x7ef937d3, 0x7efbecf9,
    0x7efe9ceb, 0x7f0147ae, 0x7f03ed4a, 0x7f068dc4, 0x7f092922, 0x7f0bbf69,
    0x7f0e50a1, 0x7f10dcce, 0x7f1363f7, 0x7f15e622, 0x7f186355, 0x7f1adb95,
    0x7f1d4ee9, 0x7f1fbd57, 0x7f2226e4, 0x7f248b96, 0x7f26eb74, 0x7f294683,
    0x7f2b9cc9, 0x7f2dee4d, 0x7f303b13, 0x7f328322, 0x7f34c680, 0x7f370533,
    0x7f393f40, 0x7f3b74ad, 0x7f3da581, 0x7f3fd1c1, 0x7f41f972, 0x7f441c9c,
    0x7f463b43, 0x7f48556d, 0x7f4a6b21, 0x7f4c7c64, 0x7f4e893c, 0x7f5091ae,
    0x7f5295c1, 0x7f54957a, 0x7f5690e0, 0x7f5887f7, 0x7f5a7ac5, 0x7f5c6951,
    0x7f5e53a0, 0x7f6039b8, 0x7f621b9e, 0x7f63f958, 0x7f65d2ed, 0x7f67a861,
    0x7f6979ba, 0x7f6b46ff, 0x7f6d1034, 0x7f6ed560, 0x7f709687, 0x7f7253b1,
    0x7f740ce1, 0x7f75c21f, 0x7f777370, 0x7f7920d8, 0x7f7aca5f, 0x7f7c7008,
    0x7f7e11db, 0x7f7fafdd, 0x7f814a13, 0x7f82e082, 0x7f847331, 0x7f860224,
    0x7f878d62, 0x7f8914f0, 0x7f8a98d4, 0x7f8c1912, 0x7f8d95b0, 0x7f8f0eb5,
    0x7f908425, 0x7f91f605, 0x7f93645c, 0x7f94cf2f, 0x7f963683, 0x7f979a5d,
    0x7f98fac4, 0x7f9a57bb, 0x7f9bb14a, 0x7f9d0775, 0x7f9e5a41, 0x7f9fa9b4,
    0x7fa0f5d3, 0x7fa23ea4, 0x7fa3842b, 0x7fa4c66f, 0x7fa60575, 0x7fa74141,
    0x7fa879d9, 0x7fa9af42, 0x7faae182, 0x7fac109e, 0x7fad3c9a, 0x7fae657d,
    0x7faf8b4c, 0x7fb0ae0b, 0x7fb1cdc0, 0x7fb2ea70, 0x7fb40420, 0x7fb51ad5,
    0x7fb62e95, 0x7fb73f64, 0x7fb84d48, 0x7fb95846, 0x7fba6062, 0x7fbb65a2,
    0x7fbc680c, 0x7fbd67a3, 0x7fbe646d, 0x7fbf5e70, 0x7fc055af, 0x7fc14a31,
    0x7fc23bf9, 0x7fc32b0d, 0x7fc41773, 0x7fc5012e, 0x7fc5e844, 0x7fc6ccba,
    0x7fc7ae94, 0x7fc88dd8, 0x7fc96a8a, 0x7fca44af, 0x7fcb1c4c, 0x7fcbf167,
    0x7fccc403, 0x7fcd9425, 0x7fce61d3, 0x7fcf2d11, 0x7fcff5e3, 0x7fd0bc4f,
    0x7fd1805a, 0x7fd24207, 0x7fd3015c, 0x7fd3be5d, 0x7fd47910, 0x7fd53178,
    0x7fd5e79b, 0x7fd69b7c, 0x7fd74d21, 0x7fd7fc8e, 0x7fd8a9c8, 0x7fd954d4,
    0x7fd9fdb5, 0x7fdaa471, 0x7fdb490b, 0x7fdbeb89, 0x7fdc8bef, 0x7fdd2a42,
    0x7fddc685, 0x7fde60be, 0x7fdef8f0, 0x7fdf8f20, 0x7fe02353, 0x7fe0b58d,
    0x7fe145d3, 0x7fe1d428, 0x7fe26091, 0x7fe2eb12, 0x7fe373b0, 0x7fe3fa6f,
    0x7fe47f53, 0x7fe50260, 0x7fe5839b, 0x7fe60308, 0x7fe680ab, 0x7fe6fc88,
    0x7fe776a4, 0x7fe7ef02, 0x7fe865a7, 0x7fe8da97, 0x7fe94dd6, 0x7fe9bf68,
    0x7fea2f51, 0x7fea9d95, 0x7feb0a39, 0x7feb7540, 0x7febdeae, 0x7fec4687,
    0x7fecaccf, 0x7fed118b, 0x7fed74be, 0x7fedd66c, 0x7fee3698, 0x7fee9548,
    0x7feef27e, 0x7fef4e3f, 0x7fefa88e, 0x7ff0016f, 0x7ff058e7, 0x7ff0aef8,
    0x7ff103a6, 0x7ff156f6, 0x7ff1a8eb, 0x7ff1f988, 0x7ff248d2, 0x7ff296cc,
    0x7ff2e37a, 0x7ff32edf, 0x7ff378ff, 0x7ff3c1de, 0x7ff4097e, 0x7ff44fe5,
    0x7ff49515, 0x7ff4d911, 0x7ff51bde, 0x7ff55d7f, 0x7ff59df7, 0x7ff5dd4a,
    0x7ff61b7b, 0x7ff6588d, 0x7ff69485, 0x7ff6cf65, 0x7ff70930, 0x7ff741eb,
    0x7ff77998, 0x7ff7b03b, 0x7ff7e5d7, 0x7ff81a6f, 0x7ff84e06, 0x7ff880a1,
    0x7ff8b241, 0x7ff8e2ea, 0x7ff912a0, 0x7ff94165, 0x7ff96f3d, 0x7ff99c2b,
    0x7ff9c831, 0x7ff9f354, 0x7ffa1d95, 0x7ffa46f9, 0x7ffa6f81, 0x7ffa9731,
    0x7ffabe0d, 0x7ffae416, 0x7ffb0951, 0x7ffb2dbf, 0x7ffb5164, 0x7ffb7442,
    0x7ffb965d, 0x7ffbb7b8, 0x7ffbd854, 0x7ffbf836, 0x7ffc175f, 0x7ffc35d3,
    0x7ffc5394, 0x7ffc70a5, 0x7ffc8d09, 0x7ffca8c2, 0x7ffcc3d4, 0x7ffcde3f,
    0x7ffcf809, 0x7ffd1132, 0x7ffd29be, 0x7ffd41ae, 0x7ffd5907, 0x7ffd6fc9,
    0x7ffd85f9, 0x7ffd9b97, 0x7ffdb0a7, 0x7ffdc52b, 0x7ffdd926, 0x7ffdec99,
    0x7ffdff88, 0x7ffe11f4, 0x7ffe23e0, 0x7ffe354f, 0x7ffe4642, 0x7ffe56bc,
    0x7ffe66bf, 0x7ffe764e, 0x7ffe856a, 0x7ffe9416, 0x7ffea254, 0x7ffeb026,
    0x7ffebd8e, 0x7ffeca8f, 0x7ffed72a, 0x7ffee362, 0x7ffeef38, 0x7ffefaaf,
    0x7fff05c9, 0x7fff1087, 0x7fff1aec, 0x7fff24f9, 0x7fff2eb1, 0x7fff3816,
    0x7fff4128, 0x7fff49eb, 0x7fff5260, 0x7fff5a88, 0x7fff6266, 0x7fff69fc,
    0x7fff714b, 0x7fff7854, 0x7fff7f1a, 0x7fff859f, 0x7fff8be3, 0x7fff91ea,
    0x7fff97b3, 0x7fff9d41, 0x7fffa296, 0x7fffa7b3, 0x7fffac99, 0x7fffb14b,
    0x7fffb5c9, 0x7fffba15, 0x7fffbe31, 0x7fffc21d, 0x7fffc5dc, 0x7fffc96f,
    0x7fffccd8, 0x7fffd016, 0x7fffd32d, 0x7fffd61c, 0x7fffd8e7, 0x7fffdb8d,
    0x7fffde0f, 0x7fffe071, 0x7fffe2b1, 0x7fffe4d2, 0x7fffe6d5, 0x7fffe8bb,
    0x7fffea85, 0x7fffec34, 0x7fffedc9, 0x7fffef45, 0x7ffff0aa, 0x7ffff1f7,
    0x7ffff330, 0x7ffff453, 0x7ffff562, 0x7ffff65f, 0x7ffff749, 0x7ffff823,
    0x7ffff8ec, 0x7ffff9a6, 0x7ffffa51, 0x7ffffaee, 0x7ffffb7e, 0x7ffffc02,
    0x7ffffc7a, 0x7ffffce7, 0x7ffffd4a, 0x7ffffda3, 0x7ffffdf4, 0x7ffffe3c,
    0x7ffffe7c, 0x7ffffeb6, 0x7ffffee8, 0x7fffff15, 0x7fffff3c, 0x7fffff5e,
    0x7fffff7b, 0x7fffff95, 0x7fffffaa, 0x7fffffbc, 0x7fffffcb, 0x7fffffd7,
    0x7fffffe2, 0x7fffffea, 0x7ffffff0, 0x7ffffff5, 0x7ffffff9, 0x7ffffffb,
    0x7ffffffd, 0x7ffffffe, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff,
    0x7fffffff, 0x7fffffff,
};

const  int32_t DecoderVorbisNative::vwin8192[4096] = {
    0x0000007c, 0x0000045c, 0x00000c1d, 0x000017bd, 0x0000273e, 0x00003a9f,
    0x000051e0, 0x00006d02, 0x00008c03, 0x0000aee5, 0x0000d5a7, 0x00010049,
    0x00012ecb, 0x0001612d, 0x00019770, 0x0001d193, 0x00020f96, 0x00025178,
    0x0002973c, 0x0002e0df, 0x00032e62, 0x00037fc5, 0x0003d509, 0x00042e2c,
    0x00048b30, 0x0004ec13, 0x000550d7, 0x0005b97a, 0x000625fe, 0x00069661,
    0x00070aa4, 0x000782c8, 0x0007fecb, 0x00087eae, 0x00090271, 0x00098a14,
    0x000a1597, 0x000aa4f9, 0x000b383b, 0x000bcf5d, 0x000c6a5f, 0x000d0941,
    0x000dac02, 0x000e52a3, 0x000efd23, 0x000fab84, 0x00105dc3, 0x001113e3,
    0x0011cde2, 0x00128bc0, 0x00134d7e, 0x0014131b, 0x0014dc98, 0x0015a9f4,
    0x00167b30, 0x0017504a, 0x00182945, 0x0019061e, 0x0019e6d7, 0x001acb6f,
    0x001bb3e6, 0x001ca03c, 0x001d9071, 0x001e8485, 0x001f7c79, 0x0020784b,
    0x002177fc, 0x00227b8c, 0x002382fb, 0x00248e49, 0x00259d76, 0x0026b081,
    0x0027c76b, 0x0028e234, 0x002a00dc, 0x002b2361, 0x002c49c6, 0x002d7409,
    0x002ea22a, 0x002fd42a, 0x00310a08, 0x003243c5, 0x00338160, 0x0034c2d9,
    0x00360830, 0x00375165, 0x00389e78, 0x0039ef6a, 0x003b4439, 0x003c9ce6,
    0x003df971, 0x003f59da, 0x0040be20, 0x00422645, 0x00439247, 0x00450226,
    0x004675e3, 0x0047ed7e, 0x004968f5, 0x004ae84b, 0x004c6b7d, 0x004df28d,
    0x004f7d7a, 0x00510c44, 0x00529eeb, 0x00543570, 0x0055cfd1, 0x00576e0f,
    0x00591029, 0x005ab621, 0x005c5ff5, 0x005e0da6, 0x005fbf33, 0x0061749d,
    0x00632de4, 0x0064eb06, 0x0066ac05, 0x006870e0, 0x006a3998, 0x006c062b,
    0x006dd69b, 0x006faae6, 0x0071830d, 0x00735f10, 0x00753eef, 0x007722a9,
    0x00790a3f, 0x007af5b1, 0x007ce4fe, 0x007ed826, 0x0080cf29, 0x0082ca08,
    0x0084c8c2, 0x0086cb57, 0x0088d1c7, 0x008adc11, 0x008cea37, 0x008efc37,
    0x00911212, 0x00932bc7, 0x00954957, 0x00976ac2, 0x00999006, 0x009bb925,
    0x009de61e, 0x00a016f1, 0x00a24b9e, 0x00a48425, 0x00a6c086, 0x00a900c0,
    0x00ab44d4, 0x00ad8cc2, 0x00afd889, 0x00b22829, 0x00b47ba2, 0x00b6d2f5,
    0x00b92e21, 0x00bb8d26, 0x00bdf004, 0x00c056ba, 0x00c2c149, 0x00c52fb1,
    0x00c7a1f1, 0x00ca180a, 0x00cc91fb, 0x00cf0fc5, 0x00d19166, 0x00d416df,
    0x00d6a031, 0x00d92d5a, 0x00dbbe5b, 0x00de5333, 0x00e0ebe3, 0x00e3886b,
    0x00e628c9, 0x00e8ccff, 0x00eb750c, 0x00ee20f0, 0x00f0d0ab, 0x00f3843d,
    0x00f63ba5, 0x00f8f6e4, 0x00fbb5fa, 0x00fe78e5, 0x01013fa7, 0x01040a3f,
    0x0106d8ae, 0x0109aaf2, 0x010c810c, 0x010f5afb, 0x011238c0, 0x01151a5b,
    0x0117ffcb, 0x011ae910, 0x011dd62a, 0x0120c719, 0x0123bbdd, 0x0126b476,
    0x0129b0e4, 0x012cb126, 0x012fb53c, 0x0132bd27, 0x0135c8e6, 0x0138d879,
    0x013bebdf, 0x013f031a, 0x01421e28, 0x01453d0a, 0x01485fbf, 0x014b8648,
    0x014eb0a4, 0x0151ded2, 0x015510d4, 0x015846a8, 0x015b8050, 0x015ebdc9,
    0x0161ff15, 0x01654434, 0x01688d24, 0x016bd9e6, 0x016f2a7b, 0x01727ee1,
    0x0175d718, 0x01793321, 0x017c92fc, 0x017ff6a7, 0x01835e24, 0x0186c972,
    0x018a3890, 0x018dab7f, 0x0191223f, 0x01949ccf, 0x01981b2f, 0x019b9d5f,
    0x019f235f, 0x01a2ad2f, 0x01a63acf, 0x01a9cc3e, 0x01ad617c, 0x01b0fa8a,
    0x01b49767, 0x01b83813, 0x01bbdc8d, 0x01bf84d6, 0x01c330ee, 0x01c6e0d4,
    0x01ca9488, 0x01ce4c0b, 0x01d2075b, 0x01d5c679, 0x01d98964, 0x01dd501d,
    0x01e11aa3, 0x01e4e8f6, 0x01e8bb17, 0x01ec9104, 0x01f06abd, 0x01f44844,
    0x01f82996, 0x01fc0eb5, 0x01fff7a0, 0x0203e456, 0x0207d4d9, 0x020bc926,
    0x020fc140, 0x0213bd24, 0x0217bcd4, 0x021bc04e, 0x021fc793, 0x0223d2a3,
    0x0227e17d, 0x022bf421, 0x02300a90, 0x023424c8, 0x023842ca, 0x023c6495,
    0x02408a2a, 0x0244b389, 0x0248e0b0, 0x024d11a0, 0x02514659, 0x02557eda,
    0x0259bb24, 0x025dfb35, 0x02623f0f, 0x026686b1, 0x026ad21a, 0x026f214b,
    0x02737443, 0x0277cb02, 0x027c2588, 0x028083d5, 0x0284e5e9, 0x02894bc2,
    0x028db562, 0x029222c8, 0x029693f4, 0x029b08e6, 0x029f819d, 0x02a3fe19,
    0x02a87e5b, 0x02ad0261, 0x02b18a2c, 0x02b615bb, 0x02baa50f, 0x02bf3827,
    0x02c3cf03, 0x02c869a3, 0x02cd0807, 0x02d1aa2d, 0x02d65017, 0x02daf9c4,
    0x02dfa734, 0x02e45866, 0x02e90d5b, 0x02edc612, 0x02f2828b, 0x02f742c6,
    0x02fc06c3, 0x0300ce80, 0x030599ff, 0x030a6940, 0x030f3c40, 0x03141302,
    0x0318ed84, 0x031dcbc6, 0x0322adc8, 0x0327938a, 0x032c7d0c, 0x03316a4c,
    0x03365b4d, 0x033b500c, 0x03404889, 0x034544c6, 0x034a44c0, 0x034f4879,
    0x03544ff0, 0x03595b24, 0x035e6a16, 0x03637cc5, 0x03689331, 0x036dad5a,
    0x0372cb40, 0x0377ece2, 0x037d1240, 0x03823b5a, 0x03876830, 0x038c98c1,
    0x0391cd0e, 0x03970516, 0x039c40d8, 0x03a18055, 0x03a6c38d, 0x03ac0a7f,
    0x03b1552b, 0x03b6a390, 0x03bbf5af, 0x03c14b88, 0x03c6a519, 0x03cc0263,
    0x03d16366, 0x03d6c821, 0x03dc3094, 0x03e19cc0, 0x03e70ca2, 0x03ec803d,
    0x03f1f78e, 0x03f77296, 0x03fcf155, 0x040273cb, 0x0407f9f7, 0x040d83d9,
    0x04131170, 0x0418a2bd, 0x041e37c0, 0x0423d077, 0x04296ce4, 0x042f0d04,
    0x0434b0da, 0x043a5863, 0x044003a0, 0x0445b290, 0x044b6534, 0x04511b8b,
    0x0456d595, 0x045c9352, 0x046254c1, 0x046819e1, 0x046de2b4, 0x0473af39,
    0x04797f6e, 0x047f5355, 0x04852aec, 0x048b0635, 0x0490e52d, 0x0496c7d6,
    0x049cae2e, 0x04a29836, 0x04a885ed, 0x04ae7753, 0x04b46c68, 0x04ba652b,
    0x04c0619d, 0x04c661bc, 0x04cc658a, 0x04d26d04, 0x04d8782c, 0x04de8701,
    0x04e49983, 0x04eaafb0, 0x04f0c98a, 0x04f6e710, 0x04fd0842, 0x05032d1e,
    0x050955a6, 0x050f81d8, 0x0515b1b5, 0x051be53d, 0x05221c6e, 0x05285748,
    0x052e95cd, 0x0534d7fa, 0x053b1dd0, 0x0541674e, 0x0547b475, 0x054e0544,
    0x055459bb, 0x055ab1d9, 0x05610d9e, 0x05676d0a, 0x056dd01c, 0x057436d5,
    0x057aa134, 0x05810f38, 0x058780e2, 0x058df631, 0x05946f25, 0x059aebbe,
    0x05a16bfa, 0x05a7efdb, 0x05ae775f, 0x05b50287, 0x05bb9152, 0x05c223c0,
    0x05c8b9d0, 0x05cf5382, 0x05d5f0d6, 0x05dc91cc, 0x05e33663, 0x05e9de9c,
    0x05f08a75, 0x05f739ee, 0x05fded07, 0x0604a3c0, 0x060b5e19, 0x06121c11,
    0x0618dda8, 0x061fa2dd, 0x06266bb1, 0x062d3822, 0x06340831, 0x063adbde,
    0x0641b328, 0x06488e0e, 0x064f6c91, 0x06564eaf, 0x065d346a, 0x06641dc0,
    0x066b0ab1, 0x0671fb3d, 0x0678ef64, 0x067fe724, 0x0686e27f, 0x068de173,
    0x0694e400, 0x069bea27, 0x06a2f3e6, 0x06aa013d, 0x06b1122c, 0x06b826b3,
    0x06bf3ed1, 0x06c65a86, 0x06cd79d1, 0x06d49cb3, 0x06dbc32b, 0x06e2ed38,
    0x06ea1adb, 0x06f14c13, 0x06f880df, 0x06ffb940, 0x0706f535, 0x070e34bd,
    0x071577d9, 0x071cbe88, 0x072408c9, 0x072b569d, 0x0732a802, 0x0739fcf9,
    0x07415582, 0x0748b19b, 0x07501145, 0x0757747f, 0x075edb49, 0x076645a3,
    0x076db38c, 0x07752503, 0x077c9a09, 0x0784129e, 0x078b8ec0, 0x07930e70,
    0x079a91ac, 0x07a21876, 0x07a9a2cc, 0x07b130ad, 0x07b8c21b, 0x07c05714,
    0x07c7ef98, 0x07cf8ba6, 0x07d72b3f, 0x07dece62, 0x07e6750e, 0x07ee1f43,
    0x07f5cd01, 0x07fd7e48, 0x08053316, 0x080ceb6d, 0x0814a74a, 0x081c66af,
    0x0824299a, 0x082bf00c, 0x0833ba03, 0x083b8780, 0x08435882, 0x084b2d09,
    0x08530514, 0x085ae0a3, 0x0862bfb6, 0x086aa24c, 0x08728865, 0x087a7201,
    0x08825f1e, 0x088a4fbe, 0x089243de, 0x089a3b80, 0x08a236a2, 0x08aa3545,
    0x08b23767, 0x08ba3d09, 0x08c2462a, 0x08ca52c9, 0x08d262e7, 0x08da7682,
    0x08e28d9c, 0x08eaa832, 0x08f2c645, 0x08fae7d4, 0x09030cdf, 0x090b3566,
    0x09136168, 0x091b90e5, 0x0923c3dc, 0x092bfa4d, 0x09343437, 0x093c719b,
    0x0944b277, 0x094cf6cc, 0x09553e99, 0x095d89dd, 0x0965d899, 0x096e2acb,
    0x09768073, 0x097ed991, 0x09873625, 0x098f962e, 0x0997f9ac, 0x09a0609e,
    0x09a8cb04, 0x09b138dd, 0x09b9aa29, 0x09c21ee8, 0x09ca9719, 0x09d312bc,
    0x09db91d0, 0x09e41456, 0x09ec9a4b, 0x09f523b1, 0x09fdb087, 0x0a0640cc,
    0x0a0ed47f, 0x0a176ba2, 0x0a200632, 0x0a28a42f, 0x0a31459a, 0x0a39ea72,
    0x0a4292b5, 0x0a4b3e65, 0x0a53ed80, 0x0a5ca006, 0x0a6555f7, 0x0a6e0f51,
    0x0a76cc16, 0x0a7f8c44, 0x0a884fda, 0x0a9116d9, 0x0a99e140, 0x0aa2af0e,
    0x0aab8043, 0x0ab454df, 0x0abd2ce1, 0x0ac60849, 0x0acee716, 0x0ad7c948,
    0x0ae0aedf, 0x0ae997d9, 0x0af28437, 0x0afb73f7, 0x0b04671b, 0x0b0d5da0,
    0x0b165788, 0x0b1f54d0, 0x0b285579, 0x0b315983, 0x0b3a60ec, 0x0b436bb5,
    0x0b4c79dd, 0x0b558b63, 0x0b5ea048, 0x0b67b88a, 0x0b70d429, 0x0b79f324,
    0x0b83157c, 0x0b8c3b30, 0x0b95643f, 0x0b9e90a8, 0x0ba7c06c, 0x0bb0f38a,
    0x0bba2a01, 0x0bc363d1, 0x0bcca0f9, 0x0bd5e17a, 0x0bdf2552, 0x0be86c81,
    0x0bf1b706, 0x0bfb04e2, 0x0c045613, 0x0c0daa99, 0x0c170274, 0x0c205da3,
    0x0c29bc25, 0x0c331dfb, 0x0c3c8323, 0x0c45eb9e, 0x0c4f576a, 0x0c58c688,
    0x0c6238f6, 0x0c6baeb5, 0x0c7527c3, 0x0c7ea421, 0x0c8823cd, 0x0c91a6c8,
    0x0c9b2d10, 0x0ca4b6a6, 0x0cae4389, 0x0cb7d3b8, 0x0cc16732, 0x0ccafdf8,
    0x0cd49809, 0x0cde3564, 0x0ce7d609, 0x0cf179f7, 0x0cfb212e, 0x0d04cbad,
    0x0d0e7974, 0x0d182a83, 0x0d21ded8, 0x0d2b9673, 0x0d355154, 0x0d3f0f7b,
    0x0d48d0e6, 0x0d529595, 0x0d5c5d88, 0x0d6628be, 0x0d6ff737, 0x0d79c8f2,
    0x0d839dee, 0x0d8d762c, 0x0d9751aa, 0x0da13068, 0x0dab1266, 0x0db4f7a3,
    0x0dbee01e, 0x0dc8cbd8, 0x0dd2bace, 0x0ddcad02, 0x0de6a272, 0x0df09b1e,
    0x0dfa9705, 0x0e049627, 0x0e0e9883, 0x0e189e19, 0x0e22a6e8, 0x0e2cb2f0,
    0x0e36c230, 0x0e40d4a8, 0x0e4aea56, 0x0e55033b, 0x0e5f1f56, 0x0e693ea7,
    0x0e73612c, 0x0e7d86e5, 0x0e87afd3, 0x0e91dbf3, 0x0e9c0b47, 0x0ea63dcc,
    0x0eb07383, 0x0ebaac6b, 0x0ec4e883, 0x0ecf27cc, 0x0ed96a44, 0x0ee3afea,
    0x0eedf8bf, 0x0ef844c2, 0x0f0293f2, 0x0f0ce64e, 0x0f173bd6, 0x0f21948a,
    0x0f2bf069, 0x0f364f72, 0x0f40b1a5, 0x0f4b1701, 0x0f557f86, 0x0f5feb32,
    0x0f6a5a07, 0x0f74cc02, 0x0f7f4124, 0x0f89b96b, 0x0f9434d8, 0x0f9eb369,
    0x0fa9351e, 0x0fb3b9f7, 0x0fbe41f3, 0x0fc8cd11, 0x0fd35b51, 0x0fddecb2,
    0x0fe88134, 0x0ff318d6, 0x0ffdb397, 0x10085177, 0x1012f275, 0x101d9691,
    0x10283dca, 0x1032e81f, 0x103d9591, 0x1048461e, 0x1052f9c5, 0x105db087,
    0x10686a62, 0x10732756, 0x107de763, 0x1088aa87, 0x109370c2, 0x109e3a14,
    0x10a9067c, 0x10b3d5f9, 0x10bea88b, 0x10c97e31, 0x10d456eb, 0x10df32b8,
    0x10ea1197, 0x10f4f387, 0x10ffd889, 0x110ac09b, 0x1115abbe, 0x112099ef,
    0x112b8b2f, 0x11367f7d, 0x114176d9, 0x114c7141, 0x11576eb6, 0x11626f36,
    0x116d72c1, 0x11787957, 0x118382f6, 0x118e8f9e, 0x11999f4f, 0x11a4b208,
    0x11afc7c7, 0x11bae08e, 0x11c5fc5a, 0x11d11b2c, 0x11dc3d02, 0x11e761dd,
    0x11f289ba, 0x11fdb49b, 0x1208e27e, 0x12141362, 0x121f4748, 0x122a7e2d,
    0x1235b812, 0x1240f4f6, 0x124c34d9, 0x125777b9, 0x1262bd96, 0x126e0670,
    0x12795245, 0x1284a115, 0x128ff2e0, 0x129b47a5, 0x12a69f63, 0x12b1fa19,
    0x12bd57c7, 0x12c8b86c, 0x12d41c08, 0x12df829a, 0x12eaec21, 0x12f6589d,
    0x1301c80c, 0x130d3a6f, 0x1318afc4, 0x1324280b, 0x132fa344, 0x133b216d,
    0x1346a286, 0x1352268e, 0x135dad85, 0x1369376a, 0x1374c43c, 0x138053fb,
    0x138be6a5, 0x13977c3b, 0x13a314bc, 0x13aeb026, 0x13ba4e79, 0x13c5efb5,
    0x13d193d9, 0x13dd3ae4, 0x13e8e4d6, 0x13f491ad, 0x1400416a, 0x140bf40b,
    0x1417a98f, 0x142361f7, 0x142f1d41, 0x143adb6d, 0x14469c7a, 0x14526067,
    0x145e2734, 0x1469f0df, 0x1475bd69, 0x14818cd0, 0x148d5f15, 0x14993435,
    0x14a50c31, 0x14b0e708, 0x14bcc4b8, 0x14c8a542, 0x14d488a5, 0x14e06edf,
    0x14ec57f1, 0x14f843d9, 0x15043297, 0x1510242b, 0x151c1892, 0x15280fcd,
    0x153409dc, 0x154006bc, 0x154c066e, 0x155808f1, 0x15640e44, 0x15701666,
    0x157c2157, 0x15882f16, 0x15943fa2, 0x15a052fb, 0x15ac691f, 0x15b8820f,
    0x15c49dc8, 0x15d0bc4c, 0x15dcdd98, 0x15e901ad, 0x15f52888, 0x1601522b,
    0x160d7e93, 0x1619adc1, 0x1625dfb3, 0x16321469, 0x163e4be2, 0x164a861d,
    0x1656c31a, 0x166302d8, 0x166f4555, 0x167b8a92, 0x1687d28e, 0x16941d47,
    0x16a06abe, 0x16acbaf0, 0x16b90ddf, 0x16c56388, 0x16d1bbeb, 0x16de1708,
    0x16ea74dd, 0x16f6d56a, 0x170338ae, 0x170f9ea8, 0x171c0758, 0x172872bd,
    0x1734e0d6, 0x174151a2, 0x174dc520, 0x175a3b51, 0x1766b432, 0x17732fc4,
    0x177fae05, 0x178c2ef4, 0x1798b292, 0x17a538dd, 0x17b1c1d4, 0x17be4d77,
    0x17cadbc5, 0x17d76cbc, 0x17e4005e, 0x17f096a7, 0x17fd2f98, 0x1809cb31,
    0x1816696f, 0x18230a53, 0x182faddc, 0x183c5408, 0x1848fcd8, 0x1855a849,
    0x1862565d, 0x186f0711, 0x187bba64, 0x18887057, 0x189528e9, 0x18a1e418,
    0x18aea1e3, 0x18bb624b, 0x18c8254e, 0x18d4eaeb, 0x18e1b321, 0x18ee7df1,
    0x18fb4b58, 0x19081b57, 0x1914edec, 0x1921c317, 0x192e9ad6, 0x193b7529,
    0x19485210, 0x19553189, 0x19621393, 0x196ef82e, 0x197bdf59, 0x1988c913,
    0x1995b55c, 0x19a2a432, 0x19af9595, 0x19bc8983, 0x19c97ffd, 0x19d67900,
    0x19e3748e, 0x19f072a3, 0x19fd7341, 0x1a0a7665, 0x1a177c10, 0x1a248440,
    0x1a318ef4, 0x1a3e9c2c, 0x1a4babe7, 0x1a58be24, 0x1a65d2e2, 0x1a72ea20,
    0x1a8003de, 0x1a8d201a, 0x1a9a3ed5, 0x1aa7600c, 0x1ab483bf, 0x1ac1a9ee,
    0x1aced297, 0x1adbfdba, 0x1ae92b56, 0x1af65b69, 0x1b038df4, 0x1b10c2f5,
    0x1b1dfa6b, 0x1b2b3456, 0x1b3870b5, 0x1b45af87, 0x1b52f0ca, 0x1b60347f,
    0x1b6d7aa4, 0x1b7ac339, 0x1b880e3c, 0x1b955bad, 0x1ba2ab8b, 0x1baffdd5,
    0x1bbd528a, 0x1bcaa9a9, 0x1bd80332, 0x1be55f24, 0x1bf2bd7d, 0x1c001e3d,
    0x1c0d8164, 0x1c1ae6ef, 0x1c284edf, 0x1c35b932, 0x1c4325e7, 0x1c5094fe,
    0x1c5e0677, 0x1c6b7a4f, 0x1c78f086, 0x1c86691b, 0x1c93e40d, 0x1ca1615c,
    0x1caee107, 0x1cbc630c, 0x1cc9e76b, 0x1cd76e23, 0x1ce4f733, 0x1cf2829a,
    0x1d001057, 0x1d0da06a, 0x1d1b32d1, 0x1d28c78c, 0x1d365e9a, 0x1d43f7f9,
    0x1d5193a9, 0x1d5f31aa, 0x1d6cd1f9, 0x1d7a7497, 0x1d881982, 0x1d95c0ba,
    0x1da36a3d, 0x1db1160a, 0x1dbec422, 0x1dcc7482, 0x1dda272b, 0x1de7dc1a,
    0x1df59350, 0x1e034ccb, 0x1e11088a, 0x1e1ec68c, 0x1e2c86d1, 0x1e3a4958,
    0x1e480e20, 0x1e55d527, 0x1e639e6d, 0x1e7169f1, 0x1e7f37b2, 0x1e8d07b0,
    0x1e9ad9e8, 0x1ea8ae5b, 0x1eb68507, 0x1ec45dec, 0x1ed23908, 0x1ee0165b,
    0x1eedf5e4, 0x1efbd7a1, 0x1f09bb92, 0x1f17a1b6, 0x1f258a0d, 0x1f337494,
    0x1f41614b, 0x1f4f5032, 0x1f5d4147, 0x1f6b3489, 0x1f7929f7, 0x1f872192,
    0x1f951b56, 0x1fa31744, 0x1fb1155b, 0x1fbf159a, 0x1fcd17ff, 0x1fdb1c8b,
    0x1fe9233b, 0x1ff72c0f, 0x20053706, 0x20134420, 0x2021535a, 0x202f64b4,
    0x203d782e, 0x204b8dc6, 0x2059a57c, 0x2067bf4e, 0x2075db3b, 0x2083f943,
    0x20921964, 0x20a03b9e, 0x20ae5fef, 0x20bc8657, 0x20caaed5, 0x20d8d967,
    0x20e7060e, 0x20f534c7, 0x21036592, 0x2111986e, 0x211fcd59, 0x212e0454,
    0x213c3d5d, 0x214a7873, 0x2158b594, 0x2166f4c1, 0x217535f8, 0x21837938,
    0x2191be81, 0x21a005d0, 0x21ae4f26, 0x21bc9a81, 0x21cae7e0, 0x21d93743,
    0x21e788a8, 0x21f5dc0e, 0x22043174, 0x221288da, 0x2220e23e, 0x222f3da0,
    0x223d9afe, 0x224bfa58, 0x225a5bac, 0x2268bef9, 0x2277243f, 0x22858b7d,
    0x2293f4b0, 0x22a25fda, 0x22b0ccf8, 0x22bf3c09, 0x22cdad0d, 0x22dc2002,
    0x22ea94e8, 0x22f90bbe, 0x23078482, 0x2315ff33, 0x23247bd1, 0x2332fa5b,
    0x23417acf, 0x234ffd2c, 0x235e8173, 0x236d07a0, 0x237b8fb4, 0x238a19ae,
    0x2398a58c, 0x23a7334d, 0x23b5c2f1, 0x23c45477, 0x23d2e7dd, 0x23e17d22,
    0x23f01446, 0x23fead47, 0x240d4825, 0x241be4dd, 0x242a8371, 0x243923dd,
    0x2447c622, 0x24566a3e, 0x24651031, 0x2473b7f8, 0x24826194, 0x24910d03,
    0x249fba44, 0x24ae6957, 0x24bd1a39, 0x24cbccea, 0x24da816a, 0x24e937b7,
    0x24f7efcf, 0x2506a9b3, 0x25156560, 0x252422d6, 0x2532e215, 0x2541a31a,
    0x255065e4, 0x255f2a74, 0x256df0c7, 0x257cb8dd, 0x258b82b4, 0x259a4e4c,
    0x25a91ba4, 0x25b7eaba, 0x25c6bb8e, 0x25d58e1e, 0x25e46269, 0x25f3386e,
    0x2602102d, 0x2610e9a4, 0x261fc4d3, 0x262ea1b7, 0x263d8050, 0x264c609e,
    0x265b429e, 0x266a2650, 0x26790bb3, 0x2687f2c6, 0x2696db88, 0x26a5c5f7,
    0x26b4b213, 0x26c39fda, 0x26d28f4c, 0x26e18067, 0x26f0732b, 0x26ff6796,
    0x270e5da7, 0x271d555d, 0x272c4eb7, 0x273b49b5, 0x274a4654, 0x27594495,
    0x27684475, 0x277745f4, 0x27864910, 0x27954dc9, 0x27a4541e, 0x27b35c0d,
    0x27c26596, 0x27d170b7, 0x27e07d6f, 0x27ef8bbd, 0x27fe9ba0, 0x280dad18,
    0x281cc022, 0x282bd4be, 0x283aeaeb, 0x284a02a7, 0x28591bf2, 0x286836cb,
    0x28775330, 0x28867120, 0x2895909b, 0x28a4b19e, 0x28b3d42a, 0x28c2f83d,
    0x28d21dd5, 0x28e144f3, 0x28f06d94, 0x28ff97b8, 0x290ec35d, 0x291df082,
    0x292d1f27, 0x293c4f4a, 0x294b80eb, 0x295ab407, 0x2969e89e, 0x29791eaf,
    0x29885639, 0x29978f3b, 0x29a6c9b3, 0x29b605a0, 0x29c54302, 0x29d481d7,
    0x29e3c21e, 0x29f303d6, 0x2a0246fd, 0x2a118b94, 0x2a20d198, 0x2a301909,
    0x2a3f61e6, 0x2a4eac2c, 0x2a5df7dc, 0x2a6d44f4, 0x2a7c9374, 0x2a8be359,
    0x2a9b34a2, 0x2aaa8750, 0x2ab9db60, 0x2ac930d1, 0x2ad887a3, 0x2ae7dfd3,
    0x2af73962, 0x2b06944e, 0x2b15f096, 0x2b254e38, 0x2b34ad34, 0x2b440d89,
    0x2b536f34, 0x2b62d236, 0x2b72368d, 0x2b819c38, 0x2b910336, 0x2ba06b86,
    0x2bafd526, 0x2bbf4015, 0x2bceac53, 0x2bde19de, 0x2bed88b5, 0x2bfcf8d7,
    0x2c0c6a43, 0x2c1bdcf7, 0x2c2b50f3, 0x2c3ac635, 0x2c4a3cbd, 0x2c59b488,
    0x2c692d97, 0x2c78a7e7, 0x2c882378, 0x2c97a049, 0x2ca71e58, 0x2cb69da4,
    0x2cc61e2c, 0x2cd59ff0, 0x2ce522ed, 0x2cf4a723, 0x2d042c90, 0x2d13b334,
    0x2d233b0d, 0x2d32c41a, 0x2d424e5a, 0x2d51d9cc, 0x2d61666e, 0x2d70f440,
    0x2d808340, 0x2d90136e, 0x2d9fa4c7, 0x2daf374c, 0x2dbecafa, 0x2dce5fd1,
    0x2dddf5cf, 0x2ded8cf4, 0x2dfd253d, 0x2e0cbeab, 0x2e1c593b, 0x2e2bf4ed,
    0x2e3b91c0, 0x2e4b2fb1, 0x2e5acec1, 0x2e6a6eee, 0x2e7a1037, 0x2e89b29b,
    0x2e995618, 0x2ea8faad, 0x2eb8a05a, 0x2ec8471c, 0x2ed7eef4, 0x2ee797df,
    0x2ef741dc, 0x2f06eceb, 0x2f16990a, 0x2f264639, 0x2f35f475, 0x2f45a3bd,
    0x2f555412, 0x2f650570, 0x2f74b7d8, 0x2f846b48, 0x2f941fbe, 0x2fa3d53a,
    0x2fb38bbb, 0x2fc3433f, 0x2fd2fbc5, 0x2fe2b54c, 0x2ff26fd3, 0x30022b58,
    0x3011e7db, 0x3021a55a, 0x303163d4, 0x30412348, 0x3050e3b5, 0x3060a519,
    0x30706773, 0x30802ac3, 0x308fef06, 0x309fb43d, 0x30af7a65, 0x30bf417d,
    0x30cf0985, 0x30ded27a, 0x30ee9c5d, 0x30fe672b, 0x310e32e3, 0x311dff85,
    0x312dcd0f, 0x313d9b80, 0x314d6ad7, 0x315d3b12, 0x316d0c30, 0x317cde31,
    0x318cb113, 0x319c84d4, 0x31ac5974, 0x31bc2ef1, 0x31cc054b, 0x31dbdc7f,
    0x31ebb48e, 0x31fb8d74, 0x320b6733, 0x321b41c7, 0x322b1d31, 0x323af96e,
    0x324ad67e, 0x325ab45f, 0x326a9311, 0x327a7291, 0x328a52e0, 0x329a33fb,
    0x32aa15e1, 0x32b9f892, 0x32c9dc0c, 0x32d9c04d, 0x32e9a555, 0x32f98b22,
    0x330971b4, 0x33195909, 0x3329411f, 0x333929f6, 0x3349138c, 0x3358fde1,
    0x3368e8f2, 0x3378d4c0, 0x3388c147, 0x3398ae89, 0x33a89c82, 0x33b88b32,
    0x33c87a98, 0x33d86ab2, 0x33e85b80, 0x33f84d00, 0x34083f30, 0x34183210,
    0x3428259f, 0x343819db, 0x34480ec3, 0x34580455, 0x3467fa92, 0x3477f176,
    0x3487e902, 0x3497e134, 0x34a7da0a, 0x34b7d384, 0x34c7cda0, 0x34d7c85e,
    0x34e7c3bb, 0x34f7bfb7, 0x3507bc50, 0x3517b985, 0x3527b756, 0x3537b5c0,
    0x3547b4c3, 0x3557b45d, 0x3567b48d, 0x3577b552, 0x3587b6aa, 0x3597b895,
    0x35a7bb12, 0x35b7be1e, 0x35c7c1b9, 0x35d7c5e1, 0x35e7ca96, 0x35f7cfd6,
    0x3607d5a0, 0x3617dbf3, 0x3627e2cd, 0x3637ea2d, 0x3647f212, 0x3657fa7b,
    0x36680366, 0x36780cd2, 0x368816bf, 0x3698212b, 0x36a82c14, 0x36b83779,
    0x36c8435a, 0x36d84fb4, 0x36e85c88, 0x36f869d2, 0x37087793, 0x371885c9,
    0x37289473, 0x3738a38f, 0x3748b31d, 0x3758c31a, 0x3768d387, 0x3778e461,
    0x3788f5a7, 0x37990759, 0x37a91975, 0x37b92bf9, 0x37c93ee4, 0x37d95236,
    0x37e965ed, 0x37f97a08, 0x38098e85, 0x3819a363, 0x3829b8a2, 0x3839ce3f,
    0x3849e43a, 0x3859fa91, 0x386a1143, 0x387a284f, 0x388a3fb4, 0x389a5770,
    0x38aa6f83, 0x38ba87ea, 0x38caa0a5, 0x38dab9b2, 0x38ead311, 0x38faecbf,
    0x390b06bc, 0x391b2107, 0x392b3b9e, 0x393b5680, 0x394b71ac, 0x395b8d20,
    0x396ba8dc, 0x397bc4dd, 0x398be124, 0x399bfdae, 0x39ac1a7a, 0x39bc3788,
    0x39cc54d5, 0x39dc7261, 0x39ec902a, 0x39fcae2f, 0x3a0ccc70, 0x3a1ceaea,
    0x3a2d099c, 0x3a3d2885, 0x3a4d47a5, 0x3a5d66f9, 0x3a6d8680, 0x3a7da63a,
    0x3a8dc625, 0x3a9de63f, 0x3aae0688, 0x3abe26fe, 0x3ace47a0, 0x3ade686d,
    0x3aee8963, 0x3afeaa82, 0x3b0ecbc7, 0x3b1eed32, 0x3b2f0ec2, 0x3b3f3075,
    0x3b4f524a, 0x3b5f7440, 0x3b6f9656, 0x3b7fb889, 0x3b8fdada, 0x3b9ffd46,
    0x3bb01fce, 0x3bc0426e, 0x3bd06526, 0x3be087f6, 0x3bf0aada, 0x3c00cdd4,
    0x3c10f0e0, 0x3c2113fe, 0x3c31372d, 0x3c415a6b, 0x3c517db7, 0x3c61a110,
    0x3c71c475, 0x3c81e7e4, 0x3c920b5c, 0x3ca22edc, 0x3cb25262, 0x3cc275ee,
    0x3cd2997e, 0x3ce2bd11, 0x3cf2e0a6, 0x3d03043b, 0x3d1327cf, 0x3d234b61,
    0x3d336ef0, 0x3d43927a, 0x3d53b5ff, 0x3d63d97c, 0x3d73fcf1, 0x3d84205c,
    0x3d9443bd, 0x3da46711, 0x3db48a58, 0x3dc4ad91, 0x3dd4d0ba, 0x3de4f3d1,
    0x3df516d7, 0x3e0539c9, 0x3e155ca6, 0x3e257f6d, 0x3e35a21d, 0x3e45c4b4,
    0x3e55e731, 0x3e660994, 0x3e762bda, 0x3e864e03, 0x3e96700d, 0x3ea691f7,
    0x3eb6b3bf, 0x3ec6d565, 0x3ed6f6e8, 0x3ee71845, 0x3ef7397c, 0x3f075a8c,
    0x3f177b73, 0x3f279c30, 0x3f37bcc2, 0x3f47dd27, 0x3f57fd5f, 0x3f681d68,
    0x3f783d40, 0x3f885ce7, 0x3f987c5c, 0x3fa89b9c, 0x3fb8baa7, 0x3fc8d97c,
    0x3fd8f819, 0x3fe9167e, 0x3ff934a8, 0x40095296, 0x40197049, 0x40298dbd,
    0x4039aaf2, 0x4049c7e7, 0x4059e49a, 0x406a010a, 0x407a1d36, 0x408a391d,
    0x409a54bd, 0x40aa7015, 0x40ba8b25, 0x40caa5ea, 0x40dac063, 0x40eada90,
    0x40faf46e, 0x410b0dfe, 0x411b273d, 0x412b402a, 0x413b58c4, 0x414b710a,
    0x415b88fa, 0x416ba093, 0x417bb7d5, 0x418bcebe, 0x419be54c, 0x41abfb7e,
    0x41bc1153, 0x41cc26ca, 0x41dc3be2, 0x41ec5099, 0x41fc64ef, 0x420c78e1,
    0x421c8c6f, 0x422c9f97, 0x423cb258, 0x424cc4b2, 0x425cd6a2, 0x426ce827,
    0x427cf941, 0x428d09ee, 0x429d1a2c, 0x42ad29fb, 0x42bd3959, 0x42cd4846,
    0x42dd56bf, 0x42ed64c3, 0x42fd7252, 0x430d7f6a, 0x431d8c0a, 0x432d9831,
    0x433da3dd, 0x434daf0d, 0x435db9c0, 0x436dc3f5, 0x437dcdab, 0x438dd6df,
    0x439ddf92, 0x43ade7c1, 0x43bdef6c, 0x43cdf691, 0x43ddfd2f, 0x43ee0345,
    0x43fe08d2, 0x440e0dd4, 0x441e124b, 0x442e1634, 0x443e198f, 0x444e1c5a,
    0x445e1e95, 0x446e203e, 0x447e2153, 0x448e21d5, 0x449e21c0, 0x44ae2115,
    0x44be1fd1, 0x44ce1df4, 0x44de1b7d, 0x44ee186a, 0x44fe14ba, 0x450e106b,
    0x451e0b7e, 0x452e05ef, 0x453dffbf, 0x454df8eb, 0x455df173, 0x456de956,
    0x457de092, 0x458dd726, 0x459dcd10, 0x45adc251, 0x45bdb6e5, 0x45cdaacd,
    0x45dd9e06, 0x45ed9091, 0x45fd826a, 0x460d7392, 0x461d6407, 0x462d53c8,
    0x463d42d4, 0x464d3129, 0x465d1ec6, 0x466d0baa, 0x467cf7d3, 0x468ce342,
    0x469ccdf3, 0x46acb7e7, 0x46bca11c, 0x46cc8990, 0x46dc7143, 0x46ec5833,
    0x46fc3e5f, 0x470c23c6, 0x471c0867, 0x472bec40, 0x473bcf50, 0x474bb196,
    0x475b9311, 0x476b73c0, 0x477b53a1, 0x478b32b4, 0x479b10f6, 0x47aaee67,
    0x47bacb06, 0x47caa6d1, 0x47da81c7, 0x47ea5be7, 0x47fa3530, 0x480a0da1,
    0x4819e537, 0x4829bbf3, 0x483991d3, 0x484966d6, 0x48593afb, 0x48690e3f,
    0x4878e0a3, 0x4888b225, 0x489882c4, 0x48a8527e, 0x48b82153, 0x48c7ef41,
    0x48d7bc47, 0x48e78863, 0x48f75396, 0x49071ddc, 0x4916e736, 0x4926afa2,
    0x4936771f, 0x49463dac, 0x49560347, 0x4965c7ef, 0x49758ba4, 0x49854e63,
    0x4995102c, 0x49a4d0fe, 0x49b490d7, 0x49c44fb6, 0x49d40d9a, 0x49e3ca82,
    0x49f3866c, 0x4a034159, 0x4a12fb45, 0x4a22b430, 0x4a326c19, 0x4a4222ff,
    0x4a51d8e1, 0x4a618dbd, 0x4a714192, 0x4a80f45f, 0x4a90a623, 0x4aa056dd,
    0x4ab0068b, 0x4abfb52c, 0x4acf62c0, 0x4adf0f44, 0x4aeebab9, 0x4afe651c,
    0x4b0e0e6c, 0x4b1db6a9, 0x4b2d5dd1, 0x4b3d03e2, 0x4b4ca8dd, 0x4b5c4cbf,
    0x4b6bef88, 0x4b7b9136, 0x4b8b31c8, 0x4b9ad13d, 0x4baa6f93, 0x4bba0ccb,
    0x4bc9a8e2, 0x4bd943d7, 0x4be8dda9, 0x4bf87658, 0x4c080de1, 0x4c17a444,
    0x4c27397f, 0x4c36cd92, 0x4c46607b, 0x4c55f239, 0x4c6582cb, 0x4c75122f,
    0x4c84a065, 0x4c942d6c, 0x4ca3b942, 0x4cb343e6, 0x4cc2cd57, 0x4cd25594,
    0x4ce1dc9c, 0x4cf1626d, 0x4d00e707, 0x4d106a68, 0x4d1fec8f, 0x4d2f6d7a,
    0x4d3eed2a, 0x4d4e6b9d, 0x4d5de8d1, 0x4d6d64c5, 0x4d7cdf79, 0x4d8c58eb,
    0x4d9bd11a, 0x4dab4804, 0x4dbabdaa, 0x4dca3209, 0x4dd9a520, 0x4de916ef,
    0x4df88774, 0x4e07f6ae, 0x4e17649c, 0x4e26d13c, 0x4e363c8f, 0x4e45a692,
    0x4e550f44, 0x4e6476a4, 0x4e73dcb2, 0x4e83416c, 0x4e92a4d1, 0x4ea206df,
    0x4eb16796, 0x4ec0c6f5, 0x4ed024fa, 0x4edf81a5, 0x4eeedcf3, 0x4efe36e5,
    0x4f0d8f79, 0x4f1ce6ad, 0x4f2c3c82, 0x4f3b90f4, 0x4f4ae405, 0x4f5a35b1,
    0x4f6985fa, 0x4f78d4dc, 0x4f882257, 0x4f976e6a, 0x4fa6b914, 0x4fb60254,
    0x4fc54a28, 0x4fd49090, 0x4fe3d58b, 0x4ff31917, 0x50025b33, 0x50119bde,
    0x5020db17, 0x503018dd, 0x503f552f, 0x504e900b, 0x505dc971, 0x506d0160,
    0x507c37d7, 0x508b6cd3, 0x509aa055, 0x50a9d25b, 0x50b902e4, 0x50c831ef,
    0x50d75f7b, 0x50e68b87, 0x50f5b612, 0x5104df1a, 0x5114069f, 0x51232ca0,
    0x5132511a, 0x5141740f, 0x5150957b, 0x515fb55f, 0x516ed3b8, 0x517df087,
    0x518d0bca, 0x519c257f, 0x51ab3da7, 0x51ba543f, 0x51c96947, 0x51d87cbd,
    0x51e78ea1, 0x51f69ef1, 0x5205adad, 0x5214bad3, 0x5223c662, 0x5232d05a,
    0x5241d8b9, 0x5250df7d, 0x525fe4a7, 0x526ee835, 0x527dea26, 0x528cea78,
    0x529be92c, 0x52aae63f, 0x52b9e1b0, 0x52c8db80, 0x52d7d3ac, 0x52e6ca33,
    0x52f5bf15, 0x5304b251, 0x5313a3e5, 0x532293d0, 0x53318212, 0x53406ea8,
    0x534f5993, 0x535e42d2, 0x536d2a62, 0x537c1043, 0x538af475, 0x5399d6f6,
    0x53a8b7c4, 0x53b796e0, 0x53c67447, 0x53d54ffa, 0x53e429f6, 0x53f3023b,
    0x5401d8c8, 0x5410ad9c, 0x541f80b5, 0x542e5213, 0x543d21b5, 0x544bef9a,
    0x545abbc0, 0x54698627, 0x54784ece, 0x548715b3, 0x5495dad6, 0x54a49e35,
    0x54b35fd0, 0x54c21fa6, 0x54d0ddb5, 0x54df99fd, 0x54ee547c, 0x54fd0d32,
    0x550bc41d, 0x551a793d, 0x55292c91, 0x5537de16, 0x55468dce, 0x55553bb6,
    0x5563e7cd, 0x55729213, 0x55813a87, 0x558fe127, 0x559e85f2, 0x55ad28e9,
    0x55bbca08, 0x55ca6950, 0x55d906c0, 0x55e7a257, 0x55f63c13, 0x5604d3f4,
    0x561369f8, 0x5621fe1f, 0x56309067, 0x563f20d1, 0x564daf5a, 0x565c3c02,
    0x566ac6c7, 0x56794faa, 0x5687d6a8, 0x56965bc1, 0x56a4def4, 0x56b36040,
    0x56c1dfa4, 0x56d05d1f, 0x56ded8af, 0x56ed5255, 0x56fbca0f, 0x570a3fdc,
    0x5718b3bc, 0x572725ac, 0x573595ad, 0x574403bd, 0x57526fdb, 0x5760da07,
    0x576f423f, 0x577da883, 0x578c0cd1, 0x579a6f29, 0x57a8cf8a, 0x57b72df2,
    0x57c58a61, 0x57d3e4d6, 0x57e23d50, 0x57f093cd, 0x57fee84e, 0x580d3ad1,
    0x581b8b54, 0x5829d9d8, 0x5838265c, 0x584670dd, 0x5854b95c, 0x5862ffd8,
    0x5871444f, 0x587f86c1, 0x588dc72c, 0x589c0591, 0x58aa41ed, 0x58b87c40,
    0x58c6b489, 0x58d4eac7, 0x58e31ef9, 0x58f1511f, 0x58ff8137, 0x590daf40,
    0x591bdb3a, 0x592a0524, 0x59382cfc, 0x594652c2, 0x59547675, 0x59629815,
    0x5970b79f, 0x597ed513, 0x598cf071, 0x599b09b7, 0x59a920e5, 0x59b735f9,
    0x59c548f4, 0x59d359d2, 0x59e16895, 0x59ef753b, 0x59fd7fc4, 0x5a0b882d,
    0x5a198e77, 0x5a2792a0, 0x5a3594a9, 0x5a43948e, 0x5a519251, 0x5a5f8df0,
    0x5a6d876a, 0x5a7b7ebe, 0x5a8973ec, 0x5a9766f2, 0x5aa557d0, 0x5ab34685,
    0x5ac1330f, 0x5acf1d6f, 0x5add05a3, 0x5aeaebaa, 0x5af8cf84, 0x5b06b12f,
    0x5b1490ab, 0x5b226df7, 0x5b304912, 0x5b3e21fc, 0x5b4bf8b2, 0x5b59cd35,
    0x5b679f84, 0x5b756f9e, 0x5b833d82, 0x5b91092e, 0x5b9ed2a3, 0x5bac99e0,
    0x5bba5ee3, 0x5bc821ac, 0x5bd5e23a, 0x5be3a08c, 0x5bf15ca1, 0x5bff1679,
    0x5c0cce12, 0x5c1a836c, 0x5c283686, 0x5c35e760, 0x5c4395f7, 0x5c51424c,
    0x5c5eec5e, 0x5c6c942b, 0x5c7a39b4, 0x5c87dcf7, 0x5c957df3, 0x5ca31ca8,
    0x5cb0b915, 0x5cbe5338, 0x5ccbeb12, 0x5cd980a1, 0x5ce713e5, 0x5cf4a4dd,
    0x5d023387, 0x5d0fbfe4, 0x5d1d49f2, 0x5d2ad1b1, 0x5d38571f, 0x5d45da3c,
    0x5d535b08, 0x5d60d981, 0x5d6e55a7, 0x5d7bcf78, 0x5d8946f5, 0x5d96bc1c,
    0x5da42eec, 0x5db19f65, 0x5dbf0d86, 0x5dcc794e, 0x5dd9e2bd, 0x5de749d1,
    0x5df4ae8a, 0x5e0210e7, 0x5e0f70e7, 0x5e1cce8a, 0x5e2a29ce, 0x5e3782b4,
    0x5e44d93a, 0x5e522d5f, 0x5e5f7f23, 0x5e6cce85, 0x5e7a1b85, 0x5e876620,
    0x5e94ae58, 0x5ea1f42a, 0x5eaf3797, 0x5ebc789d, 0x5ec9b73c, 0x5ed6f372,
    0x5ee42d41, 0x5ef164a5, 0x5efe999f, 0x5f0bcc2f, 0x5f18fc52, 0x5f262a09,
    0x5f335553, 0x5f407e2f, 0x5f4da49d, 0x5f5ac89b, 0x5f67ea29, 0x5f750946,
    0x5f8225f2, 0x5f8f402b, 0x5f9c57f2, 0x5fa96d44, 0x5fb68023, 0x5fc3908c,
    0x5fd09e7f, 0x5fdda9fc, 0x5feab302, 0x5ff7b990, 0x6004bda5, 0x6011bf40,
    0x601ebe62, 0x602bbb09, 0x6038b534, 0x6045ace4, 0x6052a216, 0x605f94cb,
    0x606c8502, 0x607972b9, 0x60865df2, 0x609346aa, 0x60a02ce1, 0x60ad1096,
    0x60b9f1c9, 0x60c6d079, 0x60d3aca5, 0x60e0864d, 0x60ed5d70, 0x60fa320d,
    0x61070424, 0x6113d3b4, 0x6120a0bc, 0x612d6b3c, 0x613a3332, 0x6146f89f,
    0x6153bb82, 0x61607bd9, 0x616d39a5, 0x6179f4e5, 0x6186ad98, 0x619363bd,
    0x61a01753, 0x61acc85b, 0x61b976d3, 0x61c622bc, 0x61d2cc13, 0x61df72d8,
    0x61ec170c, 0x61f8b8ad, 0x620557ba, 0x6211f434, 0x621e8e18, 0x622b2568,
    0x6237ba21, 0x62444c44, 0x6250dbd0, 0x625d68c4, 0x6269f320, 0x62767ae2,
    0x6283000b, 0x628f829a, 0x629c028e, 0x62a87fe6, 0x62b4faa2, 0x62c172c2,
    0x62cde844, 0x62da5b29, 0x62e6cb6e, 0x62f33915, 0x62ffa41c, 0x630c0c83,
    0x63187248, 0x6324d56d, 0x633135ef, 0x633d93ce, 0x6349ef0b, 0x635647a3,
    0x63629d97, 0x636ef0e6, 0x637b418f, 0x63878f92, 0x6393daef, 0x63a023a4,
    0x63ac69b1, 0x63b8ad15, 0x63c4edd1, 0x63d12be3, 0x63dd674b, 0x63e9a008,
    0x63f5d61a, 0x64020980, 0x640e3a39, 0x641a6846, 0x642693a5, 0x6432bc56,
    0x643ee258, 0x644b05ab, 0x6457264e, 0x64634441, 0x646f5f83, 0x647b7814,
    0x64878df3, 0x6493a120, 0x649fb199, 0x64abbf5f, 0x64b7ca71, 0x64c3d2ce,
    0x64cfd877, 0x64dbdb69, 0x64e7dba6, 0x64f3d92b, 0x64ffd3fa, 0x650bcc11,
    0x6517c16f, 0x6523b415, 0x652fa402, 0x653b9134, 0x65477bad, 0x6553636a,
    0x655f486d, 0x656b2ab3, 0x65770a3d, 0x6582e70a, 0x658ec11a, 0x659a986d,
    0x65a66d00, 0x65b23ed5, 0x65be0deb, 0x65c9da41, 0x65d5a3d7, 0x65e16aac,
    0x65ed2ebf, 0x65f8f011, 0x6604aea1, 0x66106a6e, 0x661c2377, 0x6627d9be,
    0x66338d40, 0x663f3dfd, 0x664aebf5, 0x66569728, 0x66623f95, 0x666de53b,
    0x6679881b, 0x66852833, 0x6690c583, 0x669c600b, 0x66a7f7ca, 0x66b38cc0,
    0x66bf1eec, 0x66caae4f, 0x66d63ae6, 0x66e1c4b3, 0x66ed4bb4, 0x66f8cfea,
    0x67045153, 0x670fcfef, 0x671b4bbe, 0x6726c4bf, 0x67323af3, 0x673dae58,
    0x67491eee, 0x67548cb5, 0x675ff7ab, 0x676b5fd2, 0x6776c528, 0x678227ad,
    0x678d8761, 0x6798e443, 0x67a43e52, 0x67af958f, 0x67bae9f9, 0x67c63b8f,
    0x67d18a52, 0x67dcd640, 0x67e81f59, 0x67f3659d, 0x67fea90c, 0x6809e9a5,
    0x68152768, 0x68206254, 0x682b9a68, 0x6836cfa6, 0x6842020b, 0x684d3199,
    0x68585e4d, 0x68638829, 0x686eaf2b, 0x6879d354, 0x6884f4a2, 0x68901316,
    0x689b2eb0, 0x68a6476d, 0x68b15d50, 0x68bc7056, 0x68c78080, 0x68d28dcd,
    0x68dd983e, 0x68e89fd0, 0x68f3a486, 0x68fea65d, 0x6909a555, 0x6914a16f,
    0x691f9aa9, 0x692a9104, 0x69358480, 0x6940751b, 0x694b62d5, 0x69564daf,
    0x696135a7, 0x696c1abe, 0x6976fcf3, 0x6981dc46, 0x698cb8b6, 0x69979243,
    0x69a268ed, 0x69ad3cb4, 0x69b80d97, 0x69c2db96, 0x69cda6b0, 0x69d86ee5,
    0x69e33436, 0x69edf6a1, 0x69f8b626, 0x6a0372c5, 0x6a0e2c7e, 0x6a18e350,
    0x6a23973c, 0x6a2e4840, 0x6a38f65d, 0x6a43a191, 0x6a4e49de, 0x6a58ef42,
    0x6a6391be, 0x6a6e3151, 0x6a78cdfa, 0x6a8367ba, 0x6a8dfe90, 0x6a98927c,
    0x6aa3237d, 0x6aadb194, 0x6ab83cc0, 0x6ac2c500, 0x6acd4a55, 0x6ad7ccbf,
    0x6ae24c3c, 0x6aecc8cd, 0x6af74271, 0x6b01b929, 0x6b0c2cf4, 0x6b169dd1,
    0x6b210bc1, 0x6b2b76c2, 0x6b35ded6, 0x6b4043fc, 0x6b4aa632, 0x6b55057a,
    0x6b5f61d3, 0x6b69bb3d, 0x6b7411b7, 0x6b7e6541, 0x6b88b5db, 0x6b930385,
    0x6b9d4e3f, 0x6ba79607, 0x6bb1dadf, 0x6bbc1cc6, 0x6bc65bbb, 0x6bd097bf,
    0x6bdad0d0, 0x6be506f0, 0x6bef3a1d, 0x6bf96a58, 0x6c0397a0, 0x6c0dc1f5,
    0x6c17e957, 0x6c220dc6, 0x6c2c2f41, 0x6c364dc9, 0x6c40695c, 0x6c4a81fc,
    0x6c5497a7, 0x6c5eaa5d, 0x6c68ba1f, 0x6c72c6eb, 0x6c7cd0c3, 0x6c86d7a6,
    0x6c90db92, 0x6c9adc8a, 0x6ca4da8b, 0x6caed596, 0x6cb8cdab, 0x6cc2c2ca,
    0x6cccb4f2, 0x6cd6a424, 0x6ce0905e, 0x6cea79a1, 0x6cf45fee, 0x6cfe4342,
    0x6d0823a0, 0x6d120105, 0x6d1bdb73, 0x6d25b2e8, 0x6d2f8765, 0x6d3958ea,
    0x6d432777, 0x6d4cf30a, 0x6d56bba5, 0x6d608147, 0x6d6a43f0, 0x6d7403a0,
    0x6d7dc056, 0x6d877a13, 0x6d9130d6, 0x6d9ae4a0, 0x6da4956f, 0x6dae4345,
    0x6db7ee20, 0x6dc19601, 0x6dcb3ae7, 0x6dd4dcd3, 0x6dde7bc4, 0x6de817bb,
    0x6df1b0b6, 0x6dfb46b7, 0x6e04d9bc, 0x6e0e69c7, 0x6e17f6d5, 0x6e2180e9,
    0x6e2b0801, 0x6e348c1d, 0x6e3e0d3d, 0x6e478b62, 0x6e51068a, 0x6e5a7eb7,
    0x6e63f3e7, 0x6e6d661b, 0x6e76d552, 0x6e80418e, 0x6e89aacc, 0x6e93110f,
    0x6e9c7454, 0x6ea5d49d, 0x6eaf31e9, 0x6eb88c37, 0x6ec1e389, 0x6ecb37de,
    0x6ed48936, 0x6eddd790, 0x6ee722ee, 0x6ef06b4d, 0x6ef9b0b0, 0x6f02f315,
    0x6f0c327c, 0x6f156ee6, 0x6f1ea852, 0x6f27dec1, 0x6f311232, 0x6f3a42a5,
    0x6f43701a, 0x6f4c9a91, 0x6f55c20a, 0x6f5ee686, 0x6f680803, 0x6f712682,
    0x6f7a4203, 0x6f835a86, 0x6f8c700b, 0x6f958291, 0x6f9e921a, 0x6fa79ea4,
    0x6fb0a830, 0x6fb9aebd, 0x6fc2b24c, 0x6fcbb2dd, 0x6fd4b06f, 0x6fddab03,
    0x6fe6a299, 0x6fef9730, 0x6ff888c9, 0x70017763, 0x700a62ff, 0x70134b9c,
    0x701c313b, 0x702513dc, 0x702df37e, 0x7036d021, 0x703fa9c6, 0x7048806d,
    0x70515415, 0x705a24bf, 0x7062f26b, 0x706bbd17, 0x707484c6, 0x707d4976,
    0x70860b28, 0x708ec9dc, 0x70978591, 0x70a03e48, 0x70a8f400, 0x70b1a6bb,
    0x70ba5677, 0x70c30335, 0x70cbacf5, 0x70d453b6, 0x70dcf77a, 0x70e59840,
    0x70ee3607, 0x70f6d0d1, 0x70ff689d, 0x7107fd6b, 0x71108f3b, 0x71191e0d,
    0x7121a9e2, 0x712a32b9, 0x7132b892, 0x713b3b6e, 0x7143bb4c, 0x714c382d,
    0x7154b211, 0x715d28f7, 0x71659ce0, 0x716e0dcc, 0x71767bbb, 0x717ee6ac,
    0x71874ea1, 0x718fb399, 0x71981594, 0x71a07493, 0x71a8d094, 0x71b1299a,
    0x71b97fa2, 0x71c1d2af, 0x71ca22bf, 0x71d26fd2, 0x71dab9ea, 0x71e30106,
    0x71eb4526, 0x71f3864a, 0x71fbc472, 0x7203ff9e, 0x720c37cf, 0x72146d05,
    0x721c9f3f, 0x7224ce7e, 0x722cfac2, 0x7235240b, 0x723d4a59, 0x72456dad,
    0x724d8e05, 0x7255ab63, 0x725dc5c7, 0x7265dd31, 0x726df1a0, 0x72760315,
    0x727e1191, 0x72861d12, 0x728e259a, 0x72962b28, 0x729e2dbd, 0x72a62d59,
    0x72ae29fc, 0x72b623a5, 0x72be1a56, 0x72c60e0e, 0x72cdfece, 0x72d5ec95,
    0x72ddd764, 0x72e5bf3b, 0x72eda41a, 0x72f58601, 0x72fd64f1, 0x730540e9,
    0x730d19e9, 0x7314eff3, 0x731cc305, 0x73249321, 0x732c6046, 0x73342a75,
    0x733bf1ad, 0x7343b5ef, 0x734b773b, 0x73533591, 0x735af0f2, 0x7362a95d,
    0x736a5ed3, 0x73721153, 0x7379c0df, 0x73816d76, 0x73891719, 0x7390bdc7,
    0x73986181, 0x73a00247, 0x73a7a01a, 0x73af3af8, 0x73b6d2e4, 0x73be67dc,
    0x73c5f9e1, 0x73cd88f3, 0x73d51513, 0x73dc9e40, 0x73e4247c, 0x73eba7c5,
    0x73f3281c, 0x73faa582, 0x74021ff7, 0x7409977b, 0x74110c0d, 0x74187daf,
    0x741fec61, 0x74275822, 0x742ec0f3, 0x743626d5, 0x743d89c7, 0x7444e9c9,
    0x744c46dd, 0x7453a101, 0x745af837, 0x74624c7f, 0x74699dd8, 0x7470ec44,
    0x747837c2, 0x747f8052, 0x7486c5f5, 0x748e08ac, 0x74954875, 0x749c8552,
    0x74a3bf43, 0x74aaf648, 0x74b22a62, 0x74b95b90, 0x74c089d2, 0x74c7b52a,
    0x74cedd97, 0x74d6031a, 0x74dd25b2, 0x74e44561, 0x74eb6226, 0x74f27c02,
    0x74f992f5, 0x7500a6ff, 0x7507b820, 0x750ec659, 0x7515d1aa, 0x751cda14,
    0x7523df96, 0x752ae231, 0x7531e1e5, 0x7538deb2, 0x753fd89a, 0x7546cf9b,
    0x754dc3b7, 0x7554b4ed, 0x755ba33e, 0x75628eaa, 0x75697732, 0x75705cd5,
    0x75773f95, 0x757e1f71, 0x7584fc6a, 0x758bd67f, 0x7592adb2, 0x75998203,
    0x75a05371, 0x75a721fe, 0x75adeda9, 0x75b4b673, 0x75bb7c5c, 0x75c23f65,
    0x75c8ff8d, 0x75cfbcd6, 0x75d6773f, 0x75dd2ec8, 0x75e3e373, 0x75ea953f,
    0x75f1442d, 0x75f7f03d, 0x75fe996f, 0x76053fc5, 0x760be33d, 0x761283d8,
    0x76192197, 0x761fbc7b, 0x76265482, 0x762ce9af, 0x76337c01, 0x763a0b78,
    0x76409814, 0x764721d7, 0x764da8c1, 0x76542cd1, 0x765aae08, 0x76612c67,
    0x7667a7ee, 0x766e209d, 0x76749675, 0x767b0975, 0x7681799f, 0x7687e6f3,
    0x768e5170, 0x7694b918, 0x769b1deb, 0x76a17fe9, 0x76a7df13, 0x76ae3b68,
    0x76b494ea, 0x76baeb98, 0x76c13f74, 0x76c7907c, 0x76cddeb3, 0x76d42a18,
    0x76da72ab, 0x76e0b86d, 0x76e6fb5e, 0x76ed3b7f, 0x76f378d0, 0x76f9b352,
    0x76ffeb05, 0x77061fe8, 0x770c51fe, 0x77128145, 0x7718adbf, 0x771ed76c,
    0x7724fe4c, 0x772b225f, 0x773143a7, 0x77376223, 0x773d7dd3, 0x774396ba,
    0x7749acd5, 0x774fc027, 0x7755d0af, 0x775bde6f, 0x7761e965, 0x7767f193,
    0x776df6fa, 0x7773f998, 0x7779f970, 0x777ff681, 0x7785f0cd, 0x778be852,
    0x7791dd12, 0x7797cf0d, 0x779dbe43, 0x77a3aab6, 0x77a99465, 0x77af7b50,
    0x77b55f79, 0x77bb40e0, 0x77c11f85, 0x77c6fb68, 0x77ccd48a, 0x77d2aaec,
    0x77d87e8d, 0x77de4f6f, 0x77e41d92, 0x77e9e8f5, 0x77efb19b, 0x77f57782,
    0x77fb3aad, 0x7800fb1a, 0x7806b8ca, 0x780c73bf, 0x78122bf7, 0x7817e175,
    0x781d9438, 0x78234440, 0x7828f18f, 0x782e9c25, 0x78344401, 0x7839e925,
    0x783f8b92, 0x78452b46, 0x784ac844, 0x7850628b, 0x7855fa1c, 0x785b8ef8,
    0x7861211e, 0x7866b090, 0x786c3d4d, 0x7871c757, 0x78774ead, 0x787cd351,
    0x78825543, 0x7887d483, 0x788d5111, 0x7892caef, 0x7898421c, 0x789db69a,
    0x78a32868, 0x78a89787, 0x78ae03f8, 0x78b36dbb, 0x78b8d4d1, 0x78be393a,
    0x78c39af6, 0x78c8fa06, 0x78ce566c, 0x78d3b026, 0x78d90736, 0x78de5b9c,
    0x78e3ad58, 0x78e8fc6c, 0x78ee48d7, 0x78f3929b, 0x78f8d9b7, 0x78fe1e2c,
    0x79035ffb, 0x79089f24, 0x790ddba8, 0x79131587, 0x79184cc2, 0x791d8159,
    0x7922b34d, 0x7927e29e, 0x792d0f4d, 0x7932395a, 0x793760c6, 0x793c8591,
    0x7941a7bd, 0x7946c749, 0x794be435, 0x7950fe84, 0x79561634, 0x795b2b47,
    0x79603dbc, 0x79654d96, 0x796a5ad4, 0x796f6576, 0x79746d7e, 0x797972eb,
    0x797e75bf, 0x798375f9, 0x7988739b, 0x798d6ea5, 0x79926717, 0x79975cf2,
    0x799c5037, 0x79a140e6, 0x79a62f00, 0x79ab1a85, 0x79b00376, 0x79b4e9d3,
    0x79b9cd9d, 0x79beaed4, 0x79c38d79, 0x79c8698d, 0x79cd4310, 0x79d21a03,
    0x79d6ee66, 0x79dbc03a, 0x79e08f7f, 0x79e55c36, 0x79ea265f, 0x79eeedfc,
    0x79f3b30c, 0x79f87590, 0x79fd3589, 0x7a01f2f7, 0x7a06addc, 0x7a0b6636,
    0x7a101c08, 0x7a14cf52, 0x7a198013, 0x7a1e2e4d, 0x7a22da01, 0x7a27832f,
    0x7a2c29d7, 0x7a30cdfa, 0x7a356f99, 0x7a3a0eb4, 0x7a3eab4c, 0x7a434561,
    0x7a47dcf5, 0x7a4c7207, 0x7a510498, 0x7a5594a9, 0x7a5a223a, 0x7a5ead4d,
    0x7a6335e0, 0x7a67bbf6, 0x7a6c3f8f, 0x7a70c0ab, 0x7a753f4b, 0x7a79bb6f,
    0x7a7e3519, 0x7a82ac48, 0x7a8720fe, 0x7a8b933b, 0x7a9002ff, 0x7a94704b,
    0x7a98db20, 0x7a9d437e, 0x7aa1a967, 0x7aa60cd9, 0x7aaa6dd7, 0x7aaecc61,
    0x7ab32877, 0x7ab7821b, 0x7abbd94b, 0x7ac02e0a, 0x7ac48058, 0x7ac8d035,
    0x7acd1da3, 0x7ad168a1, 0x7ad5b130, 0x7ad9f751, 0x7ade3b05, 0x7ae27c4c,
    0x7ae6bb27, 0x7aeaf796, 0x7aef319a, 0x7af36934, 0x7af79e64, 0x7afbd12c,
    0x7b00018a, 0x7b042f81, 0x7b085b10, 0x7b0c8439, 0x7b10aafc, 0x7b14cf5a,
    0x7b18f153, 0x7b1d10e8, 0x7b212e1a, 0x7b2548e9, 0x7b296155, 0x7b2d7761,
    0x7b318b0b, 0x7b359c55, 0x7b39ab3f, 0x7b3db7cb, 0x7b41c1f8, 0x7b45c9c8,
    0x7b49cf3b, 0x7b4dd251, 0x7b51d30b, 0x7b55d16b, 0x7b59cd70, 0x7b5dc71b,
    0x7b61be6d, 0x7b65b366, 0x7b69a608, 0x7b6d9653, 0x7b718447, 0x7b756fe5,
    0x7b79592e, 0x7b7d4022, 0x7b8124c3, 0x7b850710, 0x7b88e70a, 0x7b8cc4b3,
    0x7b90a00a, 0x7b947911, 0x7b984fc8, 0x7b9c242f, 0x7b9ff648, 0x7ba3c612,
    0x7ba79390, 0x7bab5ec1, 0x7baf27a5, 0x7bb2ee3f, 0x7bb6b28e, 0x7bba7493,
    0x7bbe344e, 0x7bc1f1c1, 0x7bc5acec, 0x7bc965cf, 0x7bcd1c6c, 0x7bd0d0c3,
    0x7bd482d4, 0x7bd832a1, 0x7bdbe02a, 0x7bdf8b70, 0x7be33473, 0x7be6db34,
    0x7bea7fb4, 0x7bee21f4, 0x7bf1c1f3, 0x7bf55fb3, 0x7bf8fb35, 0x7bfc9479,
    0x7c002b7f, 0x7c03c04a, 0x7c0752d8, 0x7c0ae32b, 0x7c0e7144, 0x7c11fd23,
    0x7c1586c9, 0x7c190e36, 0x7c1c936c, 0x7c20166b, 0x7c239733, 0x7c2715c6,
    0x7c2a9224, 0x7c2e0c4e, 0x7c318444, 0x7c34fa07, 0x7c386d98, 0x7c3bdef8,
    0x7c3f4e26, 0x7c42bb25, 0x7c4625f4, 0x7c498e95, 0x7c4cf507, 0x7c50594c,
    0x7c53bb65, 0x7c571b51, 0x7c5a7913, 0x7c5dd4aa, 0x7c612e17, 0x7c64855b,
    0x7c67da76, 0x7c6b2d6a, 0x7c6e7e37, 0x7c71ccdd, 0x7c75195e, 0x7c7863ba,
    0x7c7babf1, 0x7c7ef206, 0x7c8235f7, 0x7c8577c6, 0x7c88b774, 0x7c8bf502,
    0x7c8f306f, 0x7c9269bd, 0x7c95a0ec, 0x7c98d5fe, 0x7c9c08f2, 0x7c9f39cb,
    0x7ca26887, 0x7ca59528, 0x7ca8bfb0, 0x7cabe81d, 0x7caf0e72, 0x7cb232af,
    0x7cb554d4, 0x7cb874e2, 0x7cbb92db, 0x7cbeaebe, 0x7cc1c88d, 0x7cc4e047,
    0x7cc7f5ef, 0x7ccb0984, 0x7cce1b08, 0x7cd12a7b, 0x7cd437dd, 0x7cd74330,
    0x7cda4c74, 0x7cdd53aa, 0x7ce058d3, 0x7ce35bef, 0x7ce65cff, 0x7ce95c04,
    0x7cec58ff, 0x7cef53f0, 0x7cf24cd7, 0x7cf543b7, 0x7cf8388f, 0x7cfb2b60,
    0x7cfe1c2b, 0x7d010af1, 0x7d03f7b2, 0x7d06e26f, 0x7d09cb29, 0x7d0cb1e0,
    0x7d0f9696, 0x7d12794b, 0x7d1559ff, 0x7d1838b4, 0x7d1b156a, 0x7d1df022,
    0x7d20c8dd, 0x7d239f9b, 0x7d26745e, 0x7d294725, 0x7d2c17f1, 0x7d2ee6c4,
    0x7d31b39f, 0x7d347e81, 0x7d37476b, 0x7d3a0e5f, 0x7d3cd35d, 0x7d3f9665,
    0x7d425779, 0x7d451699, 0x7d47d3c6, 0x7d4a8f01, 0x7d4d484b, 0x7d4fffa3,
    0x7d52b50c, 0x7d556885, 0x7d581a0f, 0x7d5ac9ac, 0x7d5d775c, 0x7d60231f,
    0x7d62ccf6, 0x7d6574e3, 0x7d681ae6, 0x7d6abeff, 0x7d6d612f, 0x7d700178,
    0x7d729fd9, 0x7d753c54, 0x7d77d6e9, 0x7d7a6f9a, 0x7d7d0666, 0x7d7f9b4f,
    0x7d822e55, 0x7d84bf79, 0x7d874ebc, 0x7d89dc1e, 0x7d8c67a1, 0x7d8ef144,
    0x7d91790a, 0x7d93fef2, 0x7d9682fd, 0x7d99052d, 0x7d9b8581, 0x7d9e03fb,
    0x7da0809b, 0x7da2fb62, 0x7da57451, 0x7da7eb68, 0x7daa60a8, 0x7dacd413,
    0x7daf45a9, 0x7db1b56a, 0x7db42357, 0x7db68f71, 0x7db8f9b9, 0x7dbb6230,
    0x7dbdc8d6, 0x7dc02dac, 0x7dc290b3, 0x7dc4f1eb, 0x7dc75156, 0x7dc9aef4,
    0x7dcc0ac5, 0x7dce64cc, 0x7dd0bd07, 0x7dd31379, 0x7dd56821, 0x7dd7bb01,
    0x7dda0c1a, 0x7ddc5b6b, 0x7ddea8f7, 0x7de0f4bd, 0x7de33ebe, 0x7de586fc,
    0x7de7cd76, 0x7dea122e, 0x7dec5525, 0x7dee965a, 0x7df0d5d0, 0x7df31386,
    0x7df54f7e, 0x7df789b8, 0x7df9c235, 0x7dfbf8f5, 0x7dfe2dfa, 0x7e006145,
    0x7e0292d5, 0x7e04c2ac, 0x7e06f0cb, 0x7e091d32, 0x7e0b47e1, 0x7e0d70db,
    0x7e0f981f, 0x7e11bdaf, 0x7e13e18a, 0x7e1603b3, 0x7e182429, 0x7e1a42ed,
    0x7e1c6001, 0x7e1e7b64, 0x7e209518, 0x7e22ad1d, 0x7e24c375, 0x7e26d81f,
    0x7e28eb1d, 0x7e2afc70, 0x7e2d0c17, 0x7e2f1a15, 0x7e31266a, 0x7e333115,
    0x7e353a1a, 0x7e374177, 0x7e39472e, 0x7e3b4b3f, 0x7e3d4dac, 0x7e3f4e75,
    0x7e414d9a, 0x7e434b1e, 0x7e4546ff, 0x7e474140, 0x7e4939e0, 0x7e4b30e2,
    0x7e4d2644, 0x7e4f1a09, 0x7e510c30, 0x7e52fcbc, 0x7e54ebab, 0x7e56d900,
    0x7e58c4bb, 0x7e5aaedd, 0x7e5c9766, 0x7e5e7e57, 0x7e6063b2, 0x7e624776,
    0x7e6429a5, 0x7e660a3f, 0x7e67e945, 0x7e69c6b8, 0x7e6ba299, 0x7e6d7ce7,
    0x7e6f55a5, 0x7e712cd3, 0x7e730272, 0x7e74d682, 0x7e76a904, 0x7e7879f9,
    0x7e7a4962, 0x7e7c173f, 0x7e7de392, 0x7e7fae5a, 0x7e817799, 0x7e833f50,
    0x7e85057f, 0x7e86ca27, 0x7e888d49, 0x7e8a4ee5, 0x7e8c0efd, 0x7e8dcd91,
    0x7e8f8aa1, 0x7e914630, 0x7e93003c, 0x7e94b8c8, 0x7e966fd4, 0x7e982560,
    0x7e99d96e, 0x7e9b8bfe, 0x7e9d3d10, 0x7e9eeca7, 0x7ea09ac2, 0x7ea24762,
    0x7ea3f288, 0x7ea59c35, 0x7ea7446a, 0x7ea8eb27, 0x7eaa906c, 0x7eac343c,
    0x7eadd696, 0x7eaf777b, 0x7eb116ed, 0x7eb2b4eb, 0x7eb45177, 0x7eb5ec91,
    0x7eb7863b, 0x7eb91e74, 0x7ebab53e, 0x7ebc4a99, 0x7ebdde87, 0x7ebf7107,
    0x7ec1021b, 0x7ec291c3, 0x7ec42001, 0x7ec5acd5, 0x7ec7383f, 0x7ec8c241,
    0x7eca4adb, 0x7ecbd20d, 0x7ecd57da, 0x7ecedc41, 0x7ed05f44, 0x7ed1e0e2,
    0x7ed3611d, 0x7ed4dff6, 0x7ed65d6d, 0x7ed7d983, 0x7ed95438, 0x7edacd8f,
    0x7edc4586, 0x7eddbc20, 0x7edf315c, 0x7ee0a53c, 0x7ee217c1, 0x7ee388ea,
    0x7ee4f8b9, 0x7ee6672f, 0x7ee7d44c, 0x7ee94012, 0x7eeaaa80, 0x7eec1397,
    0x7eed7b59, 0x7eeee1c6, 0x7ef046df, 0x7ef1aaa5, 0x7ef30d18, 0x7ef46e39,
    0x7ef5ce09, 0x7ef72c88, 0x7ef889b8, 0x7ef9e599, 0x7efb402c, 0x7efc9972,
    0x7efdf16b, 0x7eff4818, 0x7f009d79, 0x7f01f191, 0x7f03445f, 0x7f0495e4,
    0x7f05e620, 0x7f073516, 0x7f0882c5, 0x7f09cf2d, 0x7f0b1a51, 0x7f0c6430,
    0x7f0daccc, 0x7f0ef425, 0x7f103a3b, 0x7f117f11, 0x7f12c2a5, 0x7f1404fa,
    0x7f15460f, 0x7f1685e6, 0x7f17c47f, 0x7f1901db, 0x7f1a3dfb, 0x7f1b78e0,
    0x7f1cb28a, 0x7f1deafa, 0x7f1f2231, 0x7f20582f, 0x7f218cf5, 0x7f22c085,
    0x7f23f2de, 0x7f252401, 0x7f2653f0, 0x7f2782ab, 0x7f28b032, 0x7f29dc87,
    0x7f2b07aa, 0x7f2c319c, 0x7f2d5a5e, 0x7f2e81f0, 0x7f2fa853, 0x7f30cd88,
    0x7f31f18f, 0x7f33146a, 0x7f343619, 0x7f35569c, 0x7f3675f6, 0x7f379425,
    0x7f38b12c, 0x7f39cd0a, 0x7f3ae7c0, 0x7f3c0150, 0x7f3d19ba, 0x7f3e30fe,
    0x7f3f471e, 0x7f405c1a, 0x7f416ff3, 0x7f4282a9, 0x7f43943e, 0x7f44a4b2,
    0x7f45b405, 0x7f46c239, 0x7f47cf4e, 0x7f48db45, 0x7f49e61f, 0x7f4aefdc,
    0x7f4bf87e, 0x7f4d0004, 0x7f4e0670, 0x7f4f0bc2, 0x7f500ffb, 0x7f51131c,
    0x7f521525, 0x7f531618, 0x7f5415f4, 0x7f5514bb, 0x7f56126e, 0x7f570f0c,
    0x7f580a98, 0x7f590511, 0x7f59fe78, 0x7f5af6ce, 0x7f5bee14, 0x7f5ce44a,
    0x7f5dd972, 0x7f5ecd8b, 0x7f5fc097, 0x7f60b296, 0x7f61a389, 0x7f629370,
    0x7f63824e, 0x7f647021, 0x7f655ceb, 0x7f6648ad, 0x7f673367, 0x7f681d19,
    0x7f6905c6, 0x7f69ed6d, 0x7f6ad40f, 0x7f6bb9ad, 0x7f6c9e48, 0x7f6d81e0,
    0x7f6e6475, 0x7f6f460a, 0x7f70269d, 0x7f710631, 0x7f71e4c6, 0x7f72c25c,
    0x7f739ef4, 0x7f747a8f, 0x7f75552e, 0x7f762ed1, 0x7f770779, 0x7f77df27,
    0x7f78b5db, 0x7f798b97, 0x7f7a605a, 0x7f7b3425, 0x7f7c06fa, 0x7f7cd8d9,
    0x7f7da9c2, 0x7f7e79b7, 0x7f7f48b8, 0x7f8016c5, 0x7f80e3e0, 0x7f81b009,
    0x7f827b40, 0x7f834588, 0x7f840edf, 0x7f84d747, 0x7f859ec1, 0x7f86654d,
    0x7f872aec, 0x7f87ef9e, 0x7f88b365, 0x7f897641, 0x7f8a3832, 0x7f8af93a,
    0x7f8bb959, 0x7f8c7890, 0x7f8d36df, 0x7f8df448, 0x7f8eb0ca, 0x7f8f6c67,
    0x7f90271e, 0x7f90e0f2, 0x7f9199e2, 0x7f9251f0, 0x7f93091b, 0x7f93bf65,
    0x7f9474ce, 0x7f952958, 0x7f95dd01, 0x7f968fcd, 0x7f9741ba, 0x7f97f2ca,
    0x7f98a2fd, 0x7f995254, 0x7f9a00d0, 0x7f9aae71, 0x7f9b5b38, 0x7f9c0726,
    0x7f9cb23b, 0x7f9d5c78, 0x7f9e05de, 0x7f9eae6e, 0x7f9f5627, 0x7f9ffd0b,
    0x7fa0a31b, 0x7fa14856, 0x7fa1ecbf, 0x7fa29054, 0x7fa33318, 0x7fa3d50b,
    0x7fa4762c, 0x7fa5167e, 0x7fa5b601, 0x7fa654b5, 0x7fa6f29b, 0x7fa78fb3,
    0x7fa82bff, 0x7fa8c77f, 0x7fa96234, 0x7fa9fc1e, 0x7faa953e, 0x7fab2d94,
    0x7fabc522, 0x7fac5be8, 0x7facf1e6, 0x7fad871d, 0x7fae1b8f, 0x7faeaf3b,
    0x7faf4222, 0x7fafd445, 0x7fb065a4, 0x7fb0f641, 0x7fb1861b, 0x7fb21534,
    0x7fb2a38c, 0x7fb33124, 0x7fb3bdfb, 0x7fb44a14, 0x7fb4d56f, 0x7fb5600c,
    0x7fb5e9ec, 0x7fb6730f, 0x7fb6fb76, 0x7fb78323, 0x7fb80a15, 0x7fb8904d,
    0x7fb915cc, 0x7fb99a92, 0x7fba1ea0, 0x7fbaa1f7, 0x7fbb2497, 0x7fbba681,
    0x7fbc27b5, 0x7fbca835, 0x7fbd2801, 0x7fbda719, 0x7fbe257e, 0x7fbea331,
    0x7fbf2032, 0x7fbf9c82, 0x7fc01821, 0x7fc09311, 0x7fc10d52, 0x7fc186e4,
    0x7fc1ffc8, 0x7fc277ff, 0x7fc2ef89, 0x7fc36667, 0x7fc3dc9a, 0x7fc45221,
    0x7fc4c6ff, 0x7fc53b33, 0x7fc5aebe, 0x7fc621a0, 0x7fc693db, 0x7fc7056f,
    0x7fc7765c, 0x7fc7e6a3, 0x7fc85645, 0x7fc8c542, 0x7fc9339b, 0x7fc9a150,
    0x7fca0e63, 0x7fca7ad3, 0x7fcae6a2, 0x7fcb51cf, 0x7fcbbc5c, 0x7fcc2649,
    0x7fcc8f97, 0x7fccf846, 0x7fcd6058, 0x7fcdc7cb, 0x7fce2ea2, 0x7fce94dd,
    0x7fcefa7b, 0x7fcf5f7f, 0x7fcfc3e8, 0x7fd027b7, 0x7fd08aed, 0x7fd0ed8b,
    0x7fd14f90, 0x7fd1b0fd, 0x7fd211d4, 0x7fd27214, 0x7fd2d1bf, 0x7fd330d4,
    0x7fd38f55, 0x7fd3ed41, 0x7fd44a9a, 0x7fd4a761, 0x7fd50395, 0x7fd55f37,
    0x7fd5ba48, 0x7fd614c9, 0x7fd66eba, 0x7fd6c81b, 0x7fd720ed, 0x7fd77932,
    0x7fd7d0e8, 0x7fd82812, 0x7fd87eae, 0x7fd8d4bf, 0x7fd92a45, 0x7fd97f40,
    0x7fd9d3b0, 0x7fda2797, 0x7fda7af5, 0x7fdacdca, 0x7fdb2018, 0x7fdb71dd,
    0x7fdbc31c, 0x7fdc13d5, 0x7fdc6408, 0x7fdcb3b6, 0x7fdd02df, 0x7fdd5184,
    0x7fdd9fa5, 0x7fdded44, 0x7fde3a60, 0x7fde86fb, 0x7fded314, 0x7fdf1eac,
    0x7fdf69c4, 0x7fdfb45d, 0x7fdffe76, 0x7fe04811, 0x7fe0912e, 0x7fe0d9ce,
    0x7fe121f0, 0x7fe16996, 0x7fe1b0c1, 0x7fe1f770, 0x7fe23da4, 0x7fe2835f,
    0x7fe2c89f, 0x7fe30d67, 0x7fe351b5, 0x7fe3958c, 0x7fe3d8ec, 0x7fe41bd4,
    0x7fe45e46, 0x7fe4a042, 0x7fe4e1c8, 0x7fe522da, 0x7fe56378, 0x7fe5a3a1,
    0x7fe5e358, 0x7fe6229b, 0x7fe6616d, 0x7fe69fcc, 0x7fe6ddbb, 0x7fe71b39,
    0x7fe75847, 0x7fe794e5, 0x7fe7d114, 0x7fe80cd5, 0x7fe84827, 0x7fe8830c,
    0x7fe8bd84, 0x7fe8f78f, 0x7fe9312f, 0x7fe96a62, 0x7fe9a32b, 0x7fe9db8a,
    0x7fea137e, 0x7fea4b09, 0x7fea822b, 0x7feab8e5, 0x7feaef37, 0x7feb2521,
    0x7feb5aa4, 0x7feb8fc1, 0x7febc478, 0x7febf8ca, 0x7fec2cb6, 0x7fec603e,
    0x7fec9363, 0x7fecc623, 0x7fecf881, 0x7fed2a7c, 0x7fed5c16, 0x7fed8d4e,
    0x7fedbe24, 0x7fedee9b, 0x7fee1eb1, 0x7fee4e68, 0x7fee7dc0, 0x7feeacb9,
    0x7feedb54, 0x7fef0991, 0x7fef3771, 0x7fef64f5, 0x7fef921d, 0x7fefbee8,
    0x7fefeb59, 0x7ff0176f, 0x7ff0432a, 0x7ff06e8c, 0x7ff09995, 0x7ff0c444,
    0x7ff0ee9c, 0x7ff1189b, 0x7ff14243, 0x7ff16b94, 0x7ff1948e, 0x7ff1bd32,
    0x7ff1e581, 0x7ff20d7b, 0x7ff2351f, 0x7ff25c70, 0x7ff2836d, 0x7ff2aa17,
    0x7ff2d06d, 0x7ff2f672, 0x7ff31c24, 0x7ff34185, 0x7ff36695, 0x7ff38b55,
    0x7ff3afc4, 0x7ff3d3e4, 0x7ff3f7b4, 0x7ff41b35, 0x7ff43e69, 0x7ff4614e,
    0x7ff483e6, 0x7ff4a631, 0x7ff4c82f, 0x7ff4e9e1, 0x7ff50b47, 0x7ff52c62,
    0x7ff54d33, 0x7ff56db9, 0x7ff58df5, 0x7ff5ade7, 0x7ff5cd90, 0x7ff5ecf1,
    0x7ff60c09, 0x7ff62ada, 0x7ff64963, 0x7ff667a5, 0x7ff685a1, 0x7ff6a357,
    0x7ff6c0c7, 0x7ff6ddf1, 0x7ff6fad7, 0x7ff71778, 0x7ff733d6, 0x7ff74fef,
    0x7ff76bc6, 0x7ff78759, 0x7ff7a2ab, 0x7ff7bdba, 0x7ff7d888, 0x7ff7f315,
    0x7ff80d61, 0x7ff8276c, 0x7ff84138, 0x7ff85ac4, 0x7ff87412, 0x7ff88d20,
    0x7ff8a5f0, 0x7ff8be82, 0x7ff8d6d7, 0x7ff8eeef, 0x7ff906c9, 0x7ff91e68,
    0x7ff935cb, 0x7ff94cf2, 0x7ff963dd, 0x7ff97a8f, 0x7ff99105, 0x7ff9a742,
    0x7ff9bd45, 0x7ff9d30f, 0x7ff9e8a0, 0x7ff9fdf9, 0x7ffa131a, 0x7ffa2803,
    0x7ffa3cb4, 0x7ffa512f, 0x7ffa6573, 0x7ffa7981, 0x7ffa8d59, 0x7ffaa0fc,
    0x7ffab46a, 0x7ffac7a3, 0x7ffadaa8, 0x7ffaed78, 0x7ffb0015, 0x7ffb127f,
    0x7ffb24b6, 0x7ffb36bb, 0x7ffb488d, 0x7ffb5a2e, 0x7ffb6b9d, 0x7ffb7cdb,
    0x7ffb8de9, 0x7ffb9ec6, 0x7ffbaf73, 0x7ffbbff1, 0x7ffbd03f, 0x7ffbe05e,
    0x7ffbf04f, 0x7ffc0012, 0x7ffc0fa6, 0x7ffc1f0d, 0x7ffc2e47, 0x7ffc3d54,
    0x7ffc4c35, 0x7ffc5ae9, 0x7ffc6971, 0x7ffc77ce, 0x7ffc8600, 0x7ffc9407,
    0x7ffca1e4, 0x7ffcaf96, 0x7ffcbd1f, 0x7ffcca7e, 0x7ffcd7b4, 0x7ffce4c1,
    0x7ffcf1a5, 0x7ffcfe62, 0x7ffd0af6, 0x7ffd1763, 0x7ffd23a9, 0x7ffd2fc8,
    0x7ffd3bc1, 0x7ffd4793, 0x7ffd533f, 0x7ffd5ec5, 0x7ffd6a27, 0x7ffd7563,
    0x7ffd807a, 0x7ffd8b6e, 0x7ffd963d, 0x7ffda0e8, 0x7ffdab70, 0x7ffdb5d5,
    0x7ffdc017, 0x7ffdca36, 0x7ffdd434, 0x7ffdde0f, 0x7ffde7c9, 0x7ffdf161,
    0x7ffdfad8, 0x7ffe042f, 0x7ffe0d65, 0x7ffe167b, 0x7ffe1f71, 0x7ffe2848,
    0x7ffe30ff, 0x7ffe3997, 0x7ffe4211, 0x7ffe4a6c, 0x7ffe52a9, 0x7ffe5ac8,
    0x7ffe62c9, 0x7ffe6aae, 0x7ffe7275, 0x7ffe7a1f, 0x7ffe81ad, 0x7ffe891f,
    0x7ffe9075, 0x7ffe97b0, 0x7ffe9ece, 0x7ffea5d2, 0x7ffeacbb, 0x7ffeb38a,
    0x7ffeba3e, 0x7ffec0d8, 0x7ffec758, 0x7ffecdbf, 0x7ffed40d, 0x7ffeda41,
    0x7ffee05d, 0x7ffee660, 0x7ffeec4b, 0x7ffef21f, 0x7ffef7da, 0x7ffefd7e,
    0x7fff030b, 0x7fff0881, 0x7fff0de0, 0x7fff1328, 0x7fff185b, 0x7fff1d77,
    0x7fff227e, 0x7fff276f, 0x7fff2c4b, 0x7fff3112, 0x7fff35c4, 0x7fff3a62,
    0x7fff3eeb, 0x7fff4360, 0x7fff47c2, 0x7fff4c0f, 0x7fff504a, 0x7fff5471,
    0x7fff5885, 0x7fff5c87, 0x7fff6076, 0x7fff6452, 0x7fff681d, 0x7fff6bd6,
    0x7fff6f7d, 0x7fff7313, 0x7fff7698, 0x7fff7a0c, 0x7fff7d6f, 0x7fff80c2,
    0x7fff8404, 0x7fff8736, 0x7fff8a58, 0x7fff8d6b, 0x7fff906e, 0x7fff9362,
    0x7fff9646, 0x7fff991c, 0x7fff9be3, 0x7fff9e9c, 0x7fffa146, 0x7fffa3e2,
    0x7fffa671, 0x7fffa8f1, 0x7fffab65, 0x7fffadca, 0x7fffb023, 0x7fffb26f,
    0x7fffb4ae, 0x7fffb6e0, 0x7fffb906, 0x7fffbb20, 0x7fffbd2e, 0x7fffbf30,
    0x7fffc126, 0x7fffc311, 0x7fffc4f1, 0x7fffc6c5, 0x7fffc88f, 0x7fffca4d,
    0x7fffcc01, 0x7fffcdab, 0x7fffcf4a, 0x7fffd0e0, 0x7fffd26b, 0x7fffd3ec,
    0x7fffd564, 0x7fffd6d2, 0x7fffd838, 0x7fffd993, 0x7fffdae6, 0x7fffdc31,
    0x7fffdd72, 0x7fffdeab, 0x7fffdfdb, 0x7fffe104, 0x7fffe224, 0x7fffe33c,
    0x7fffe44d, 0x7fffe556, 0x7fffe657, 0x7fffe751, 0x7fffe844, 0x7fffe930,
    0x7fffea15, 0x7fffeaf3, 0x7fffebca, 0x7fffec9b, 0x7fffed66, 0x7fffee2a,
    0x7fffeee8, 0x7fffefa0, 0x7ffff053, 0x7ffff0ff, 0x7ffff1a6, 0x7ffff247,
    0x7ffff2e4, 0x7ffff37a, 0x7ffff40c, 0x7ffff499, 0x7ffff520, 0x7ffff5a3,
    0x7ffff621, 0x7ffff69b, 0x7ffff710, 0x7ffff781, 0x7ffff7ee, 0x7ffff857,
    0x7ffff8bb, 0x7ffff91c, 0x7ffff979, 0x7ffff9d2, 0x7ffffa27, 0x7ffffa79,
    0x7ffffac8, 0x7ffffb13, 0x7ffffb5b, 0x7ffffba0, 0x7ffffbe2, 0x7ffffc21,
    0x7ffffc5d, 0x7ffffc96, 0x7ffffccd, 0x7ffffd01, 0x7ffffd32, 0x7ffffd61,
    0x7ffffd8e, 0x7ffffdb8, 0x7ffffde0, 0x7ffffe07, 0x7ffffe2b, 0x7ffffe4d,
    0x7ffffe6d, 0x7ffffe8b, 0x7ffffea8, 0x7ffffec3, 0x7ffffedc, 0x7ffffef4,
    0x7fffff0a, 0x7fffff1f, 0x7fffff33, 0x7fffff45, 0x7fffff56, 0x7fffff66,
    0x7fffff75, 0x7fffff82, 0x7fffff8f, 0x7fffff9a, 0x7fffffa5, 0x7fffffaf,
    0x7fffffb8, 0x7fffffc0, 0x7fffffc8, 0x7fffffce, 0x7fffffd5, 0x7fffffda,
    0x7fffffdf, 0x7fffffe4, 0x7fffffe8, 0x7fffffeb, 0x7fffffef, 0x7ffffff1,
    0x7ffffff4, 0x7ffffff6, 0x7ffffff8, 0x7ffffff9, 0x7ffffffb, 0x7ffffffc,
    0x7ffffffd, 0x7ffffffd, 0x7ffffffe, 0x7fffffff, 0x7fffffff, 0x7fffffff,
    0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff,
    0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff,
};

/* {sin(2*i*PI/4096, cos(2*i*PI/4096}, with i = 0 to 512 */
const int32_t DecoderVorbisNative::sincos_lookup0[1026] = {
    0x00000000, 0x7fffffff, 0x003243f5, 0x7ffff621, 0x006487e3, 0x7fffd886,
    0x0096cbc1, 0x7fffa72c, 0x00c90f88, 0x7fff6216, 0x00fb5330, 0x7fff0943,
    0x012d96b1, 0x7ffe9cb2, 0x015fda03, 0x7ffe1c65, 0x01921d20, 0x7ffd885a,
    0x01c45ffe, 0x7ffce093, 0x01f6a297, 0x7ffc250f, 0x0228e4e2, 0x7ffb55ce,
    0x025b26d7, 0x7ffa72d1, 0x028d6870, 0x7ff97c18, 0x02bfa9a4, 0x7ff871a2,
    0x02f1ea6c, 0x7ff75370, 0x03242abf, 0x7ff62182, 0x03566a96, 0x7ff4dbd9,
    0x0388a9ea, 0x7ff38274, 0x03bae8b2, 0x7ff21553, 0x03ed26e6, 0x7ff09478,
    0x041f6480, 0x7feeffe1, 0x0451a177, 0x7fed5791, 0x0483ddc3, 0x7feb9b85,
    0x04b6195d, 0x7fe9cbc0, 0x04e8543e, 0x7fe7e841, 0x051a8e5c, 0x7fe5f108,
    0x054cc7b1, 0x7fe3e616, 0x057f0035, 0x7fe1c76b, 0x05b137df, 0x7fdf9508,
    0x05e36ea9, 0x7fdd4eec, 0x0615a48b, 0x7fdaf519, 0x0647d97c, 0x7fd8878e,
    0x067a0d76, 0x7fd6064c, 0x06ac406f, 0x7fd37153, 0x06de7262, 0x7fd0c8a3,
    0x0710a345, 0x7fce0c3e, 0x0742d311, 0x7fcb3c23, 0x077501be, 0x7fc85854,
    0x07a72f45, 0x7fc560cf, 0x07d95b9e, 0x7fc25596, 0x080b86c2, 0x7fbf36aa,
    0x083db0a7, 0x7fbc040a, 0x086fd947, 0x7fb8bdb8, 0x08a2009a, 0x7fb563b3,
    0x08d42699, 0x7fb1f5fc, 0x09064b3a, 0x7fae7495, 0x09386e78, 0x7faadf7c,
    0x096a9049, 0x7fa736b4, 0x099cb0a7, 0x7fa37a3c, 0x09cecf89, 0x7f9faa15,
    0x0a00ece8, 0x7f9bc640, 0x0a3308bd, 0x7f97cebd, 0x0a6522fe, 0x7f93c38c,
    0x0a973ba5, 0x7f8fa4b0, 0x0ac952aa, 0x7f8b7227, 0x0afb6805, 0x7f872bf3,
    0x0b2d7baf, 0x7f82d214, 0x0b5f8d9f, 0x7f7e648c, 0x0b919dcf, 0x7f79e35a,
    0x0bc3ac35, 0x7f754e80, 0x0bf5b8cb, 0x7f70a5fe, 0x0c27c389, 0x7f6be9d4,
    0x0c59cc68, 0x7f671a05, 0x0c8bd35e, 0x7f62368f, 0x0cbdd865, 0x7f5d3f75,
    0x0cefdb76, 0x7f5834b7, 0x0d21dc87, 0x7f531655, 0x0d53db92, 0x7f4de451,
    0x0d85d88f, 0x7f489eaa, 0x0db7d376, 0x7f434563, 0x0de9cc40, 0x7f3dd87c,
    0x0e1bc2e4, 0x7f3857f6, 0x0e4db75b, 0x7f32c3d1, 0x0e7fa99e, 0x7f2d1c0e,
    0x0eb199a4, 0x7f2760af, 0x0ee38766, 0x7f2191b4, 0x0f1572dc, 0x7f1baf1e,
    0x0f475bff, 0x7f15b8ee, 0x0f7942c7, 0x7f0faf25, 0x0fab272b, 0x7f0991c4,
    0x0fdd0926, 0x7f0360cb, 0x100ee8ad, 0x7efd1c3c, 0x1040c5bb, 0x7ef6c418,
    0x1072a048, 0x7ef05860, 0x10a4784b, 0x7ee9d914, 0x10d64dbd, 0x7ee34636,
    0x11082096, 0x7edc9fc6, 0x1139f0cf, 0x7ed5e5c6, 0x116bbe60, 0x7ecf1837,
    0x119d8941, 0x7ec8371a, 0x11cf516a, 0x7ec14270, 0x120116d5, 0x7eba3a39,
    0x1232d979, 0x7eb31e78, 0x1264994e, 0x7eabef2c, 0x1296564d, 0x7ea4ac58,
    0x12c8106f, 0x7e9d55fc, 0x12f9c7aa, 0x7e95ec1a, 0x132b7bf9, 0x7e8e6eb2,
    0x135d2d53, 0x7e86ddc6, 0x138edbb1, 0x7e7f3957, 0x13c0870a, 0x7e778166,
    0x13f22f58, 0x7e6fb5f4, 0x1423d492, 0x7e67d703, 0x145576b1, 0x7e5fe493,
    0x148715ae, 0x7e57dea7, 0x14b8b17f, 0x7e4fc53e, 0x14ea4a1f, 0x7e47985b,
    0x151bdf86, 0x7e3f57ff, 0x154d71aa, 0x7e37042a, 0x157f0086, 0x7e2e9cdf,
    0x15b08c12, 0x7e26221f, 0x15e21445, 0x7e1d93ea, 0x16139918, 0x7e14f242,
    0x16451a83, 0x7e0c3d29, 0x1676987f, 0x7e0374a0, 0x16a81305, 0x7dfa98a8,
    0x16d98a0c, 0x7df1a942, 0x170afd8d, 0x7de8a670, 0x173c6d80, 0x7ddf9034,
    0x176dd9de, 0x7dd6668f, 0x179f429f, 0x7dcd2981, 0x17d0a7bc, 0x7dc3d90d,
    0x1802092c, 0x7dba7534, 0x183366e9, 0x7db0fdf8, 0x1864c0ea, 0x7da77359,
    0x18961728, 0x7d9dd55a, 0x18c7699b, 0x7d9423fc, 0x18f8b83c, 0x7d8a5f40,
    0x192a0304, 0x7d808728, 0x195b49ea, 0x7d769bb5, 0x198c8ce7, 0x7d6c9ce9,
    0x19bdcbf3, 0x7d628ac6, 0x19ef0707, 0x7d58654d, 0x1a203e1b, 0x7d4e2c7f,
    0x1a517128, 0x7d43e05e, 0x1a82a026, 0x7d3980ec, 0x1ab3cb0d, 0x7d2f0e2b,
    0x1ae4f1d6, 0x7d24881b, 0x1b161479, 0x7d19eebf, 0x1b4732ef, 0x7d0f4218,
    0x1b784d30, 0x7d048228, 0x1ba96335, 0x7cf9aef0, 0x1bda74f6, 0x7ceec873,
    0x1c0b826a, 0x7ce3ceb2, 0x1c3c8b8c, 0x7cd8c1ae, 0x1c6d9053, 0x7ccda169,
    0x1c9e90b8, 0x7cc26de5, 0x1ccf8cb3, 0x7cb72724, 0x1d00843d, 0x7cabcd28,
    0x1d31774d, 0x7ca05ff1, 0x1d6265dd, 0x7c94df83, 0x1d934fe5, 0x7c894bde,
    0x1dc4355e, 0x7c7da505, 0x1df5163f, 0x7c71eaf9, 0x1e25f282, 0x7c661dbc,
    0x1e56ca1e, 0x7c5a3d50, 0x1e879d0d, 0x7c4e49b7, 0x1eb86b46, 0x7c4242f2,
    0x1ee934c3, 0x7c362904, 0x1f19f97b, 0x7c29fbee, 0x1f4ab968, 0x7c1dbbb3,
    0x1f7b7481, 0x7c116853, 0x1fac2abf, 0x7c0501d2, 0x1fdcdc1b, 0x7bf88830,
    0x200d888d, 0x7bebfb70, 0x203e300d, 0x7bdf5b94, 0x206ed295, 0x7bd2a89e,
    0x209f701c, 0x7bc5e290, 0x20d0089c, 0x7bb9096b, 0x21009c0c, 0x7bac1d31,
    0x21312a65, 0x7b9f1de6, 0x2161b3a0, 0x7b920b89, 0x219237b5, 0x7b84e61f,
    0x21c2b69c, 0x7b77ada8, 0x21f3304f, 0x7b6a6227, 0x2223a4c5, 0x7b5d039e,
    0x225413f8, 0x7b4f920e, 0x22847de0, 0x7b420d7a, 0x22b4e274, 0x7b3475e5,
    0x22e541af, 0x7b26cb4f, 0x23159b88, 0x7b190dbc, 0x2345eff8, 0x7b0b3d2c,
    0x23763ef7, 0x7afd59a4, 0x23a6887f, 0x7aef6323, 0x23d6cc87, 0x7ae159ae,
    0x24070b08, 0x7ad33d45, 0x243743fa, 0x7ac50dec, 0x24677758, 0x7ab6cba4,
    0x2497a517, 0x7aa8766f, 0x24c7cd33, 0x7a9a0e50, 0x24f7efa2, 0x7a8b9348,
    0x25280c5e, 0x7a7d055b, 0x2558235f, 0x7a6e648a, 0x2588349d, 0x7a5fb0d8,
    0x25b84012, 0x7a50ea47, 0x25e845b6, 0x7a4210d8, 0x26184581, 0x7a332490,
    0x26483f6c, 0x7a24256f, 0x26783370, 0x7a151378, 0x26a82186, 0x7a05eead,
    0x26d809a5, 0x79f6b711, 0x2707ebc7, 0x79e76ca7, 0x2737c7e3, 0x79d80f6f,
    0x27679df4, 0x79c89f6e, 0x27976df1, 0x79b91ca4, 0x27c737d3, 0x79a98715,
    0x27f6fb92, 0x7999dec4, 0x2826b928, 0x798a23b1, 0x2856708d, 0x797a55e0,
    0x288621b9, 0x796a7554, 0x28b5cca5, 0x795a820e, 0x28e5714b, 0x794a7c12,
    0x29150fa1, 0x793a6361, 0x2944a7a2, 0x792a37fe, 0x29743946, 0x7919f9ec,
    0x29a3c485, 0x7909a92d, 0x29d34958, 0x78f945c3, 0x2a02c7b8, 0x78e8cfb2,
    0x2a323f9e, 0x78d846fb, 0x2a61b101, 0x78c7aba2, 0x2a911bdc, 0x78b6fda8,
    0x2ac08026, 0x78a63d11, 0x2aefddd8, 0x789569df, 0x2b1f34eb, 0x78848414,
    0x2b4e8558, 0x78738bb3, 0x2b7dcf17, 0x786280bf, 0x2bad1221, 0x7851633b,
    0x2bdc4e6f, 0x78403329, 0x2c0b83fa, 0x782ef08b, 0x2c3ab2b9, 0x781d9b65,
    0x2c69daa6, 0x780c33b8, 0x2c98fbba, 0x77fab989, 0x2cc815ee, 0x77e92cd9,
    0x2cf72939, 0x77d78daa, 0x2d263596, 0x77c5dc01, 0x2d553afc, 0x77b417df,
    0x2d843964, 0x77a24148, 0x2db330c7, 0x7790583e, 0x2de2211e, 0x777e5cc3,
    0x2e110a62, 0x776c4edb, 0x2e3fec8b, 0x775a2e89, 0x2e6ec792, 0x7747fbce,
    0x2e9d9b70, 0x7735b6af, 0x2ecc681e, 0x77235f2d, 0x2efb2d95, 0x7710f54c,
    0x2f29ebcc, 0x76fe790e, 0x2f58a2be, 0x76ebea77, 0x2f875262, 0x76d94989,
    0x2fb5fab2, 0x76c69647, 0x2fe49ba7, 0x76b3d0b4, 0x30133539, 0x76a0f8d2,
    0x3041c761, 0x768e0ea6, 0x30705217, 0x767b1231, 0x309ed556, 0x76680376,
    0x30cd5115, 0x7654e279, 0x30fbc54d, 0x7641af3d, 0x312a31f8, 0x762e69c4,
    0x3158970e, 0x761b1211, 0x3186f487, 0x7607a828, 0x31b54a5e, 0x75f42c0b,
    0x31e39889, 0x75e09dbd, 0x3211df04, 0x75ccfd42, 0x32401dc6, 0x75b94a9c,
    0x326e54c7, 0x75a585cf, 0x329c8402, 0x7591aedd, 0x32caab6f, 0x757dc5ca,
    0x32f8cb07, 0x7569ca99, 0x3326e2c3, 0x7555bd4c, 0x3354f29b, 0x75419de7,
    0x3382fa88, 0x752d6c6c, 0x33b0fa84, 0x751928e0, 0x33def287, 0x7504d345,
    0x340ce28b, 0x74f06b9e, 0x343aca87, 0x74dbf1ef, 0x3468aa76, 0x74c7663a,
    0x34968250, 0x74b2c884, 0x34c4520d, 0x749e18cd, 0x34f219a8, 0x7489571c,
    0x351fd918, 0x74748371, 0x354d9057, 0x745f9dd1, 0x357b3f5d, 0x744aa63f,
    0x35a8e625, 0x74359cbd, 0x35d684a6, 0x74208150, 0x36041ad9, 0x740b53fb,
    0x3631a8b8, 0x73f614c0, 0x365f2e3b, 0x73e0c3a3, 0x368cab5c, 0x73cb60a8,
    0x36ba2014, 0x73b5ebd1, 0x36e78c5b, 0x73a06522, 0x3714f02a, 0x738acc9e,
    0x37424b7b, 0x73752249, 0x376f9e46, 0x735f6626, 0x379ce885, 0x73499838,
    0x37ca2a30, 0x7333b883, 0x37f76341, 0x731dc70a, 0x382493b0, 0x7307c3d0,
    0x3851bb77, 0x72f1aed9, 0x387eda8e, 0x72db8828, 0x38abf0ef, 0x72c54fc1,
    0x38d8fe93, 0x72af05a7, 0x39060373, 0x7298a9dd, 0x3932ff87, 0x72823c67,
    0x395ff2c9, 0x726bbd48, 0x398cdd32, 0x72552c85, 0x39b9bebc, 0x723e8a20,
    0x39e6975e, 0x7227d61c, 0x3a136712, 0x7211107e, 0x3a402dd2, 0x71fa3949,
    0x3a6ceb96, 0x71e35080, 0x3a99a057, 0x71cc5626, 0x3ac64c0f, 0x71b54a41,
    0x3af2eeb7, 0x719e2cd2, 0x3b1f8848, 0x7186fdde, 0x3b4c18ba, 0x716fbd68,
    0x3b78a007, 0x71586b74, 0x3ba51e29, 0x71410805, 0x3bd19318, 0x7129931f,
    0x3bfdfecd, 0x71120cc5, 0x3c2a6142, 0x70fa74fc, 0x3c56ba70, 0x70e2cbc6,
    0x3c830a50, 0x70cb1128, 0x3caf50da, 0x70b34525, 0x3cdb8e09, 0x709b67c0,
    0x3d07c1d6, 0x708378ff, 0x3d33ec39, 0x706b78e3, 0x3d600d2c, 0x70536771,
    0x3d8c24a8, 0x703b44ad, 0x3db832a6, 0x7023109a, 0x3de4371f, 0x700acb3c,
    0x3e10320d, 0x6ff27497, 0x3e3c2369, 0x6fda0cae, 0x3e680b2c, 0x6fc19385,
    0x3e93e950, 0x6fa90921, 0x3ebfbdcd, 0x6f906d84, 0x3eeb889c, 0x6f77c0b3,
    0x3f1749b8, 0x6f5f02b2, 0x3f430119, 0x6f463383, 0x3f6eaeb8, 0x6f2d532c,
    0x3f9a5290, 0x6f1461b0, 0x3fc5ec98, 0x6efb5f12, 0x3ff17cca, 0x6ee24b57,
    0x401d0321, 0x6ec92683, 0x40487f94, 0x6eaff099, 0x4073f21d, 0x6e96a99d,
    0x409f5ab6, 0x6e7d5193, 0x40cab958, 0x6e63e87f, 0x40f60dfb, 0x6e4a6e66,
    0x4121589b, 0x6e30e34a, 0x414c992f, 0x6e174730, 0x4177cfb1, 0x6dfd9a1c,
    0x41a2fc1a, 0x6de3dc11, 0x41ce1e65, 0x6dca0d14, 0x41f93689, 0x6db02d29,
    0x42244481, 0x6d963c54, 0x424f4845, 0x6d7c3a98, 0x427a41d0, 0x6d6227fa,
    0x42a5311b, 0x6d48047e, 0x42d0161e, 0x6d2dd027, 0x42faf0d4, 0x6d138afb,
    0x4325c135, 0x6cf934fc, 0x4350873c, 0x6cdece2f, 0x437b42e1, 0x6cc45698,
    0x43a5f41e, 0x6ca9ce3b, 0x43d09aed, 0x6c8f351c, 0x43fb3746, 0x6c748b3f,
    0x4425c923, 0x6c59d0a9, 0x4450507e, 0x6c3f055d, 0x447acd50, 0x6c242960,
    0x44a53f93, 0x6c093cb6, 0x44cfa740, 0x6bee3f62, 0x44fa0450, 0x6bd3316a,
    0x452456bd, 0x6bb812d1, 0x454e9e80, 0x6b9ce39b, 0x4578db93, 0x6b81a3cd,
    0x45a30df0, 0x6b66536b, 0x45cd358f, 0x6b4af279, 0x45f7526b, 0x6b2f80fb,
    0x4621647d, 0x6b13fef5, 0x464b6bbe, 0x6af86c6c, 0x46756828, 0x6adcc964,
    0x469f59b4, 0x6ac115e2, 0x46c9405c, 0x6aa551e9, 0x46f31c1a, 0x6a897d7d,
    0x471cece7, 0x6a6d98a4, 0x4746b2bc, 0x6a51a361, 0x47706d93, 0x6a359db9,
    0x479a1d67, 0x6a1987b0, 0x47c3c22f, 0x69fd614a, 0x47ed5be6, 0x69e12a8c,
    0x4816ea86, 0x69c4e37a, 0x48406e08, 0x69a88c19, 0x4869e665, 0x698c246c,
    0x48935397, 0x696fac78, 0x48bcb599, 0x69532442, 0x48e60c62, 0x69368bce,
    0x490f57ee, 0x6919e320, 0x49389836, 0x68fd2a3d, 0x4961cd33, 0x68e06129,
    0x498af6df, 0x68c387e9, 0x49b41533, 0x68a69e81, 0x49dd282a, 0x6889a4f6,
    0x4a062fbd, 0x686c9b4b, 0x4a2f2be6, 0x684f8186, 0x4a581c9e, 0x683257ab,
    0x4a8101de, 0x68151dbe, 0x4aa9dba2, 0x67f7d3c5, 0x4ad2a9e2, 0x67da79c3,
    0x4afb6c98, 0x67bd0fbd, 0x4b2423be, 0x679f95b7, 0x4b4ccf4d, 0x67820bb7,
    0x4b756f40, 0x676471c0, 0x4b9e0390, 0x6746c7d8, 0x4bc68c36, 0x67290e02,
    0x4bef092d, 0x670b4444, 0x4c177a6e, 0x66ed6aa1, 0x4c3fdff4, 0x66cf8120,
    0x4c6839b7, 0x66b187c3, 0x4c9087b1, 0x66937e91, 0x4cb8c9dd, 0x6675658c,
    0x4ce10034, 0x66573cbb, 0x4d092ab0, 0x66390422, 0x4d31494b, 0x661abbc5,
    0x4d595bfe, 0x65fc63a9, 0x4d8162c4, 0x65ddfbd3, 0x4da95d96, 0x65bf8447,
    0x4dd14c6e, 0x65a0fd0b, 0x4df92f46, 0x65826622, 0x4e210617, 0x6563bf92,
    0x4e48d0dd, 0x6545095f, 0x4e708f8f, 0x6526438f, 0x4e984229, 0x65076e25,
    0x4ebfe8a5, 0x64e88926, 0x4ee782fb, 0x64c99498, 0x4f0f1126, 0x64aa907f,
    0x4f369320, 0x648b7ce0, 0x4f5e08e3, 0x646c59bf, 0x4f857269, 0x644d2722,
    0x4faccfab, 0x642de50d, 0x4fd420a4, 0x640e9386, 0x4ffb654d, 0x63ef3290,
    0x50229da1, 0x63cfc231, 0x5049c999, 0x63b0426d, 0x5070e92f, 0x6390b34a,
    0x5097fc5e, 0x637114cc, 0x50bf031f, 0x635166f9, 0x50e5fd6d, 0x6331a9d4,
    0x510ceb40, 0x6311dd64, 0x5133cc94, 0x62f201ac, 0x515aa162, 0x62d216b3,
    0x518169a5, 0x62b21c7b, 0x51a82555, 0x6292130c, 0x51ced46e, 0x6271fa69,
    0x51f576ea, 0x6251d298, 0x521c0cc2, 0x62319b9d, 0x524295f0, 0x6211557e,
    0x5269126e, 0x61f1003f, 0x528f8238, 0x61d09be5, 0x52b5e546, 0x61b02876,
    0x52dc3b92, 0x618fa5f7, 0x53028518, 0x616f146c, 0x5328c1d0, 0x614e73da,
    0x534ef1b5, 0x612dc447, 0x537514c2, 0x610d05b7, 0x539b2af0, 0x60ec3830,
    0x53c13439, 0x60cb5bb7, 0x53e73097, 0x60aa7050, 0x540d2005, 0x60897601,
    0x5433027d, 0x60686ccf, 0x5458d7f9, 0x604754bf, 0x547ea073, 0x60262dd6,
    0x54a45be6, 0x6004f819, 0x54ca0a4b, 0x5fe3b38d, 0x54efab9c, 0x5fc26038,
    0x55153fd4, 0x5fa0fe1f, 0x553ac6ee, 0x5f7f8d46, 0x556040e2, 0x5f5e0db3,
    0x5585adad, 0x5f3c7f6b, 0x55ab0d46, 0x5f1ae274, 0x55d05faa, 0x5ef936d1,
    0x55f5a4d2, 0x5ed77c8a, 0x561adcb9, 0x5eb5b3a2, 0x56400758, 0x5e93dc1f,
    0x566524aa, 0x5e71f606, 0x568a34a9, 0x5e50015d, 0x56af3750, 0x5e2dfe29,
    0x56d42c99, 0x5e0bec6e, 0x56f9147e, 0x5de9cc33, 0x571deefa, 0x5dc79d7c,
    0x5742bc06, 0x5da5604f, 0x57677b9d, 0x5d8314b1, 0x578c2dba, 0x5d60baa7,
    0x57b0d256, 0x5d3e5237, 0x57d5696d, 0x5d1bdb65, 0x57f9f2f8, 0x5cf95638,
    0x581e6ef1, 0x5cd6c2b5, 0x5842dd54, 0x5cb420e0, 0x58673e1b, 0x5c9170bf,
    0x588b9140, 0x5c6eb258, 0x58afd6bd, 0x5c4be5b0, 0x58d40e8c, 0x5c290acc,
    0x58f838a9, 0x5c0621b2, 0x591c550e, 0x5be32a67, 0x594063b5, 0x5bc024f0,
    0x59646498, 0x5b9d1154, 0x598857b2, 0x5b79ef96, 0x59ac3cfd, 0x5b56bfbd,
    0x59d01475, 0x5b3381ce, 0x59f3de12, 0x5b1035cf, 0x5a1799d1, 0x5aecdbc5,
    0x5a3b47ab, 0x5ac973b5, 0x5a5ee79a, 0x5aa5fda5, 0x5a82799a, 0x5a82799a};

/* {sin((2*i+1*PI/4096, cos((2*i+1*PI/4096}, with i = 0 to 511 */
const int32_t DecoderVorbisNative::sincos_lookup1[1024] = {
    0x001921fb, 0x7ffffd88, 0x004b65ee, 0x7fffe9cb, 0x007da9d4, 0x7fffc251,
    0x00afeda8, 0x7fff8719, 0x00e23160, 0x7fff3824, 0x011474f6, 0x7ffed572,
    0x0146b860, 0x7ffe5f03, 0x0178fb99, 0x7ffdd4d7, 0x01ab3e97, 0x7ffd36ee,
    0x01dd8154, 0x7ffc8549, 0x020fc3c6, 0x7ffbbfe6, 0x024205e8, 0x7ffae6c7,
    0x027447b0, 0x7ff9f9ec, 0x02a68917, 0x7ff8f954, 0x02d8ca16, 0x7ff7e500,
    0x030b0aa4, 0x7ff6bcf0, 0x033d4abb, 0x7ff58125, 0x036f8a51, 0x7ff4319d,
    0x03a1c960, 0x7ff2ce5b, 0x03d407df, 0x7ff1575d, 0x040645c7, 0x7fefcca4,
    0x04388310, 0x7fee2e30, 0x046abfb3, 0x7fec7c02, 0x049cfba7, 0x7feab61a,
    0x04cf36e5, 0x7fe8dc78, 0x05017165, 0x7fe6ef1c, 0x0533ab20, 0x7fe4ee06,
    0x0565e40d, 0x7fe2d938, 0x05981c26, 0x7fe0b0b1, 0x05ca5361, 0x7fde7471,
    0x05fc89b8, 0x7fdc247a, 0x062ebf22, 0x7fd9c0ca, 0x0660f398, 0x7fd74964,
    0x06932713, 0x7fd4be46, 0x06c5598a, 0x7fd21f72, 0x06f78af6, 0x7fcf6ce8,
    0x0729bb4e, 0x7fcca6a7, 0x075bea8c, 0x7fc9ccb2, 0x078e18a7, 0x7fc6df08,
    0x07c04598, 0x7fc3dda9, 0x07f27157, 0x7fc0c896, 0x08249bdd, 0x7fbd9fd0,
    0x0856c520, 0x7fba6357, 0x0888ed1b, 0x7fb7132b, 0x08bb13c5, 0x7fb3af4e,
    0x08ed3916, 0x7fb037bf, 0x091f5d06, 0x7facac7f, 0x09517f8f, 0x7fa90d8e,
    0x0983a0a7, 0x7fa55aee, 0x09b5c048, 0x7fa1949e, 0x09e7de6a, 0x7f9dbaa0,
    0x0a19fb04, 0x7f99ccf4, 0x0a4c1610, 0x7f95cb9a, 0x0a7e2f85, 0x7f91b694,
    0x0ab0475c, 0x7f8d8de1, 0x0ae25d8d, 0x7f895182, 0x0b147211, 0x7f850179,
    0x0b4684df, 0x7f809dc5, 0x0b7895f0, 0x7f7c2668, 0x0baaa53b, 0x7f779b62,
    0x0bdcb2bb, 0x7f72fcb4, 0x0c0ebe66, 0x7f6e4a5e, 0x0c40c835, 0x7f698461,
    0x0c72d020, 0x7f64aabf, 0x0ca4d620, 0x7f5fbd77, 0x0cd6da2d, 0x7f5abc8a,
    0x0d08dc3f, 0x7f55a7fa, 0x0d3adc4e, 0x7f507fc7, 0x0d6cda53, 0x7f4b43f2,
    0x0d9ed646, 0x7f45f47b, 0x0dd0d01f, 0x7f409164, 0x0e02c7d7, 0x7f3b1aad,
    0x0e34bd66, 0x7f359057, 0x0e66b0c3, 0x7f2ff263, 0x0e98a1e9, 0x7f2a40d2,
    0x0eca90ce, 0x7f247ba5, 0x0efc7d6b, 0x7f1ea2dc, 0x0f2e67b8, 0x7f18b679,
    0x0f604faf, 0x7f12b67c, 0x0f923546, 0x7f0ca2e7, 0x0fc41876, 0x7f067bba,
    0x0ff5f938, 0x7f0040f6, 0x1027d784, 0x7ef9f29d, 0x1059b352, 0x7ef390ae,
    0x108b8c9b, 0x7eed1b2c, 0x10bd6356, 0x7ee69217, 0x10ef377d, 0x7edff570,
    0x11210907, 0x7ed94538, 0x1152d7ed, 0x7ed28171, 0x1184a427, 0x7ecbaa1a,
    0x11b66dad, 0x7ec4bf36, 0x11e83478, 0x7ebdc0c6, 0x1219f880, 0x7eb6aeca,
    0x124bb9be, 0x7eaf8943, 0x127d7829, 0x7ea85033, 0x12af33ba, 0x7ea1039b,
    0x12e0ec6a, 0x7e99a37c, 0x1312a230, 0x7e922fd6, 0x13445505, 0x7e8aa8ac,
    0x137604e2, 0x7e830dff, 0x13a7b1bf, 0x7e7b5fce, 0x13d95b93, 0x7e739e1d,
    0x140b0258, 0x7e6bc8eb, 0x143ca605, 0x7e63e03b, 0x146e4694, 0x7e5be40c,
    0x149fe3fc, 0x7e53d462, 0x14d17e36, 0x7e4bb13c, 0x1503153a, 0x7e437a9c,
    0x1534a901, 0x7e3b3083, 0x15663982, 0x7e32d2f4, 0x1597c6b7, 0x7e2a61ed,
    0x15c95097, 0x7e21dd73, 0x15fad71b, 0x7e194584, 0x162c5a3b, 0x7e109a24,
    0x165dd9f0, 0x7e07db52, 0x168f5632, 0x7dff0911, 0x16c0cef9, 0x7df62362,
    0x16f2443e, 0x7ded2a47, 0x1723b5f9, 0x7de41dc0, 0x17552422, 0x7ddafdce,
    0x17868eb3, 0x7dd1ca75, 0x17b7f5a3, 0x7dc883b4, 0x17e958ea, 0x7dbf298d,
    0x181ab881, 0x7db5bc02, 0x184c1461, 0x7dac3b15, 0x187d6c82, 0x7da2a6c6,
    0x18aec0db, 0x7d98ff17, 0x18e01167, 0x7d8f4409, 0x19115e1c, 0x7d85759f,
    0x1942a6f3, 0x7d7b93da, 0x1973ebe6, 0x7d719eba, 0x19a52ceb, 0x7d679642,
    0x19d669fc, 0x7d5d7a74, 0x1a07a311, 0x7d534b50, 0x1a38d823, 0x7d4908d9,
    0x1a6a0929, 0x7d3eb30f, 0x1a9b361d, 0x7d3449f5, 0x1acc5ef6, 0x7d29cd8c,
    0x1afd83ad, 0x7d1f3dd6, 0x1b2ea43a, 0x7d149ad5, 0x1b5fc097, 0x7d09e489,
    0x1b90d8bb, 0x7cff1af5, 0x1bc1ec9e, 0x7cf43e1a, 0x1bf2fc3a, 0x7ce94dfb,
    0x1c240786, 0x7cde4a98, 0x1c550e7c, 0x7cd333f3, 0x1c861113, 0x7cc80a0f,
    0x1cb70f43, 0x7cbcccec, 0x1ce80906, 0x7cb17c8d, 0x1d18fe54, 0x7ca618f3,
    0x1d49ef26, 0x7c9aa221, 0x1d7adb73, 0x7c8f1817, 0x1dabc334, 0x7c837ad8,
    0x1ddca662, 0x7c77ca65, 0x1e0d84f5, 0x7c6c06c0, 0x1e3e5ee5, 0x7c602fec,
    0x1e6f342c, 0x7c5445e9, 0x1ea004c1, 0x7c4848ba, 0x1ed0d09d, 0x7c3c3860,
    0x1f0197b8, 0x7c3014de, 0x1f325a0b, 0x7c23de35, 0x1f63178f, 0x7c179467,
    0x1f93d03c, 0x7c0b3777, 0x1fc4840a, 0x7bfec765, 0x1ff532f2, 0x7bf24434,
    0x2025dcec, 0x7be5ade6, 0x205681f1, 0x7bd9047c, 0x208721f9, 0x7bcc47fa,
    0x20b7bcfe, 0x7bbf7860, 0x20e852f6, 0x7bb295b0, 0x2118e3dc, 0x7ba59fee,
    0x21496fa7, 0x7b989719, 0x2179f64f, 0x7b8b7b36, 0x21aa77cf, 0x7b7e4c45,
    0x21daf41d, 0x7b710a49, 0x220b6b32, 0x7b63b543, 0x223bdd08, 0x7b564d36,
    0x226c4996, 0x7b48d225, 0x229cb0d5, 0x7b3b4410, 0x22cd12bd, 0x7b2da2fa,
    0x22fd6f48, 0x7b1feee5, 0x232dc66d, 0x7b1227d3, 0x235e1826, 0x7b044dc7,
    0x238e646a, 0x7af660c2, 0x23beab33, 0x7ae860c7, 0x23eeec78, 0x7ada4dd8,
    0x241f2833, 0x7acc27f7, 0x244f5e5c, 0x7abdef25, 0x247f8eec, 0x7aafa367,
    0x24afb9da, 0x7aa144bc, 0x24dfdf20, 0x7a92d329, 0x250ffeb7, 0x7a844eae,
    0x25401896, 0x7a75b74f, 0x25702cb7, 0x7a670d0d, 0x25a03b11, 0x7a584feb,
    0x25d0439f, 0x7a497feb, 0x26004657, 0x7a3a9d0f, 0x26304333, 0x7a2ba75a,
    0x26603a2c, 0x7a1c9ece, 0x26902b39, 0x7a0d836d, 0x26c01655, 0x79fe5539,
    0x26effb76, 0x79ef1436, 0x271fda96, 0x79dfc064, 0x274fb3ae, 0x79d059c8,
    0x277f86b5, 0x79c0e062, 0x27af53a6, 0x79b15435, 0x27df1a77, 0x79a1b545,
    0x280edb23, 0x79920392, 0x283e95a1, 0x79823f20, 0x286e49ea, 0x797267f2,
    0x289df7f8, 0x79627e08, 0x28cd9fc1, 0x79528167, 0x28fd4140, 0x79427210,
    0x292cdc6d, 0x79325006, 0x295c7140, 0x79221b4b, 0x298bffb2, 0x7911d3e2,
    0x29bb87bc, 0x790179cd, 0x29eb0957, 0x78f10d0f, 0x2a1a847b, 0x78e08dab,
    0x2a49f920, 0x78cffba3, 0x2a796740, 0x78bf56f9, 0x2aa8ced3, 0x78ae9fb0,
    0x2ad82fd2, 0x789dd5cb, 0x2b078a36, 0x788cf94c, 0x2b36ddf7, 0x787c0a36,
    0x2b662b0e, 0x786b088c, 0x2b957173, 0x7859f44f, 0x2bc4b120, 0x7848cd83,
    0x2bf3ea0d, 0x7837942b, 0x2c231c33, 0x78264849, 0x2c52478a, 0x7814e9df,
    0x2c816c0c, 0x780378f1, 0x2cb089b1, 0x77f1f581, 0x2cdfa071, 0x77e05f91,
    0x2d0eb046, 0x77ceb725, 0x2d3db928, 0x77bcfc3f, 0x2d6cbb10, 0x77ab2ee2,
    0x2d9bb5f6, 0x77994f11, 0x2dcaa9d5, 0x77875cce, 0x2df996a3, 0x7775581d,
    0x2e287c5a, 0x776340ff, 0x2e575af3, 0x77511778, 0x2e863267, 0x773edb8b,
    0x2eb502ae, 0x772c8d3a, 0x2ee3cbc1, 0x771a2c88, 0x2f128d99, 0x7707b979,
    0x2f41482e, 0x76f5340e, 0x2f6ffb7a, 0x76e29c4b, 0x2f9ea775, 0x76cff232,
    0x2fcd4c19, 0x76bd35c7, 0x2ffbe95d, 0x76aa670d, 0x302a7f3a, 0x76978605,
    0x30590dab, 0x768492b4, 0x308794a6, 0x76718d1c, 0x30b61426, 0x765e7540,
    0x30e48c22, 0x764b4b23, 0x3112fc95, 0x76380ec8, 0x31416576, 0x7624c031,
    0x316fc6be, 0x76115f63, 0x319e2067, 0x75fdec60, 0x31cc7269, 0x75ea672a,
    0x31fabcbd, 0x75d6cfc5, 0x3228ff5c, 0x75c32634, 0x32573a3f, 0x75af6a7b,
    0x32856d5e, 0x759b9c9b, 0x32b398b3, 0x7587bc98, 0x32e1bc36, 0x7573ca75,
    0x330fd7e1, 0x755fc635, 0x333debab, 0x754bafdc, 0x336bf78f, 0x7537876c,
    0x3399fb85, 0x75234ce8, 0x33c7f785, 0x750f0054, 0x33f5eb89, 0x74faa1b3,
    0x3423d78a, 0x74e63108, 0x3451bb81, 0x74d1ae55, 0x347f9766, 0x74bd199f,
    0x34ad6b32, 0x74a872e8, 0x34db36df, 0x7493ba34, 0x3508fa66, 0x747eef85,
    0x3536b5be, 0x746a12df, 0x356468e2, 0x74552446, 0x359213c9, 0x744023bc,
    0x35bfb66e, 0x742b1144, 0x35ed50c9, 0x7415ece2, 0x361ae2d3, 0x7400b69a,
    0x36486c86, 0x73eb6e6e, 0x3675edd9, 0x73d61461, 0x36a366c6, 0x73c0a878,
    0x36d0d746, 0x73ab2ab4, 0x36fe3f52, 0x73959b1b, 0x372b9ee3, 0x737ff9ae,
    0x3758f5f2, 0x736a4671, 0x37864477, 0x73548168, 0x37b38a6d, 0x733eaa96,
    0x37e0c7cc, 0x7328c1ff, 0x380dfc8d, 0x7312c7a5, 0x383b28a9, 0x72fcbb8c,
    0x38684c19, 0x72e69db7, 0x389566d6, 0x72d06e2b, 0x38c278d9, 0x72ba2cea,
    0x38ef821c, 0x72a3d9f7, 0x391c8297, 0x728d7557, 0x39497a43, 0x7276ff0d,
    0x39766919, 0x7260771b, 0x39a34f13, 0x7249dd86, 0x39d02c2a, 0x72333251,
    0x39fd0056, 0x721c7580, 0x3a29cb91, 0x7205a716, 0x3a568dd4, 0x71eec716,
    0x3a834717, 0x71d7d585, 0x3aaff755, 0x71c0d265, 0x3adc9e86, 0x71a9bdba,
    0x3b093ca3, 0x71929789, 0x3b35d1a5, 0x717b5fd3, 0x3b625d86, 0x7164169d,
    0x3b8ee03e, 0x714cbbeb, 0x3bbb59c7, 0x71354fc0, 0x3be7ca1a, 0x711dd220,
    0x3c143130, 0x7106430e, 0x3c408f03, 0x70eea28e, 0x3c6ce38a, 0x70d6f0a4,
    0x3c992ec0, 0x70bf2d53, 0x3cc5709e, 0x70a7589f, 0x3cf1a91c, 0x708f728b,
    0x3d1dd835, 0x70777b1c, 0x3d49fde1, 0x705f7255, 0x3d761a19, 0x70475839,
    0x3da22cd7, 0x702f2ccd, 0x3dce3614, 0x7016f014, 0x3dfa35c8, 0x6ffea212,
    0x3e262bee, 0x6fe642ca, 0x3e52187f, 0x6fcdd241, 0x3e7dfb73, 0x6fb5507a,
    0x3ea9d4c3, 0x6f9cbd79, 0x3ed5a46b, 0x6f841942, 0x3f016a61, 0x6f6b63d8,
    0x3f2d26a0, 0x6f529d40, 0x3f58d921, 0x6f39c57d, 0x3f8481dd, 0x6f20dc92,
    0x3fb020ce, 0x6f07e285, 0x3fdbb5ec, 0x6eeed758, 0x40074132, 0x6ed5bb10,
    0x4032c297, 0x6ebc8db0, 0x405e3a16, 0x6ea34f3d, 0x4089a7a8, 0x6e89ffb9,
    0x40b50b46, 0x6e709f2a, 0x40e064ea, 0x6e572d93, 0x410bb48c, 0x6e3daaf8,
    0x4136fa27, 0x6e24175c, 0x416235b2, 0x6e0a72c5, 0x418d6729, 0x6df0bd35,
    0x41b88e84, 0x6dd6f6b1, 0x41e3abbc, 0x6dbd1f3c, 0x420ebecb, 0x6da336dc,
    0x4239c7aa, 0x6d893d93, 0x4264c653, 0x6d6f3365, 0x428fbabe, 0x6d551858,
    0x42baa4e6, 0x6d3aec6e, 0x42e584c3, 0x6d20afac, 0x43105a50, 0x6d066215,
    0x433b2585, 0x6cec03af, 0x4365e65b, 0x6cd1947c, 0x43909ccd, 0x6cb71482,
    0x43bb48d4, 0x6c9c83c3, 0x43e5ea68, 0x6c81e245, 0x44108184, 0x6c67300b,
    0x443b0e21, 0x6c4c6d1a, 0x44659039, 0x6c319975, 0x449007c4, 0x6c16b521,
    0x44ba74bd, 0x6bfbc021, 0x44e4d71c, 0x6be0ba7b, 0x450f2edb, 0x6bc5a431,
    0x45397bf4, 0x6baa7d49, 0x4563be60, 0x6b8f45c7, 0x458df619, 0x6b73fdae,
    0x45b82318, 0x6b58a503, 0x45e24556, 0x6b3d3bcb, 0x460c5cce, 0x6b21c208,
    0x46366978, 0x6b0637c1, 0x46606b4e, 0x6aea9cf8, 0x468a624a, 0x6acef1b2,
    0x46b44e65, 0x6ab335f4, 0x46de2f99, 0x6a9769c1, 0x470805df, 0x6a7b8d1e,
    0x4731d131, 0x6a5fa010, 0x475b9188, 0x6a43a29a, 0x478546de, 0x6a2794c1,
    0x47aef12c, 0x6a0b7689, 0x47d8906d, 0x69ef47f6, 0x48022499, 0x69d3090e,
    0x482badab, 0x69b6b9d3, 0x48552b9b, 0x699a5a4c, 0x487e9e64, 0x697dea7b,
    0x48a805ff, 0x69616a65, 0x48d16265, 0x6944da10, 0x48fab391, 0x6928397e,
    0x4923f97b, 0x690b88b5, 0x494d341e, 0x68eec7b9, 0x49766373, 0x68d1f68f,
    0x499f8774, 0x68b5153a, 0x49c8a01b, 0x689823bf, 0x49f1ad61, 0x687b2224,
    0x4a1aaf3f, 0x685e106c, 0x4a43a5b0, 0x6840ee9b, 0x4a6c90ad, 0x6823bcb7,
    0x4a957030, 0x68067ac3, 0x4abe4433, 0x67e928c5, 0x4ae70caf, 0x67cbc6c0,
    0x4b0fc99d, 0x67ae54ba, 0x4b387af9, 0x6790d2b6, 0x4b6120bb, 0x677340ba,
    0x4b89badd, 0x67559eca, 0x4bb24958, 0x6737ecea, 0x4bdacc28, 0x671a2b20,
    0x4c034345, 0x66fc596f, 0x4c2baea9, 0x66de77dc, 0x4c540e4e, 0x66c0866d,
    0x4c7c622d, 0x66a28524, 0x4ca4aa41, 0x66847408, 0x4ccce684, 0x6666531d,
    0x4cf516ee, 0x66482267, 0x4d1d3b7a, 0x6629e1ec, 0x4d455422, 0x660b91af,
    0x4d6d60df, 0x65ed31b5, 0x4d9561ac, 0x65cec204, 0x4dbd5682, 0x65b0429f,
    0x4de53f5a, 0x6591b38c, 0x4e0d1c30, 0x657314cf, 0x4e34ecfc, 0x6554666d,
    0x4e5cb1b9, 0x6535a86b, 0x4e846a60, 0x6516dacd, 0x4eac16eb, 0x64f7fd98,
    0x4ed3b755, 0x64d910d1, 0x4efb4b96, 0x64ba147d, 0x4f22d3aa, 0x649b08a0,
    0x4f4a4f89, 0x647bed3f, 0x4f71bf2e, 0x645cc260, 0x4f992293, 0x643d8806,
    0x4fc079b1, 0x641e3e38, 0x4fe7c483, 0x63fee4f8, 0x500f0302, 0x63df7c4d,
    0x50363529, 0x63c0043b, 0x505d5af1, 0x63a07cc7, 0x50847454, 0x6380e5f6,
    0x50ab814d, 0x63613fcd, 0x50d281d5, 0x63418a50, 0x50f975e6, 0x6321c585,
    0x51205d7b, 0x6301f171, 0x5147388c, 0x62e20e17, 0x516e0715, 0x62c21b7e,
    0x5194c910, 0x62a219aa, 0x51bb7e75, 0x628208a1, 0x51e22740, 0x6261e866,
    0x5208c36a, 0x6241b8ff, 0x522f52ee, 0x62217a72, 0x5255d5c5, 0x62012cc2,
    0x527c4bea, 0x61e0cff5, 0x52a2b556, 0x61c06410, 0x52c91204, 0x619fe918,
    0x52ef61ee, 0x617f5f12, 0x5315a50e, 0x615ec603, 0x533bdb5d, 0x613e1df0,
    0x536204d7, 0x611d66de, 0x53882175, 0x60fca0d2, 0x53ae3131, 0x60dbcbd1,
    0x53d43406, 0x60bae7e1, 0x53fa29ed, 0x6099f505, 0x542012e1, 0x6078f344,
    0x5445eedb, 0x6057e2a2, 0x546bbdd7, 0x6036c325, 0x54917fce, 0x601594d1,
    0x54b734ba, 0x5ff457ad, 0x54dcdc96, 0x5fd30bbc, 0x5502775c, 0x5fb1b104,
    0x55280505, 0x5f90478a, 0x554d858d, 0x5f6ecf53, 0x5572f8ed, 0x5f4d4865,
    0x55985f20, 0x5f2bb2c5, 0x55bdb81f, 0x5f0a0e77, 0x55e303e6, 0x5ee85b82,
    0x5608426e, 0x5ec699e9, 0x562d73b2, 0x5ea4c9b3, 0x565297ab, 0x5e82eae5,
    0x5677ae54, 0x5e60fd84, 0x569cb7a8, 0x5e3f0194, 0x56c1b3a1, 0x5e1cf71c,
    0x56e6a239, 0x5dfade20, 0x570b8369, 0x5dd8b6a7, 0x5730572e, 0x5db680b4,
    0x57551d80, 0x5d943c4e, 0x5779d65b, 0x5d71e979, 0x579e81b8, 0x5d4f883b,
    0x57c31f92, 0x5d2d189a, 0x57e7afe4, 0x5d0a9a9a, 0x580c32a7, 0x5ce80e41,
    0x5830a7d6, 0x5cc57394, 0x58550f6c, 0x5ca2ca99, 0x58796962, 0x5c801354,
    0x589db5b3, 0x5c5d4dcc, 0x58c1f45b, 0x5c3a7a05, 0x58e62552, 0x5c179806,
    0x590a4893, 0x5bf4a7d2, 0x592e5e19, 0x5bd1a971, 0x595265df, 0x5bae9ce7,
    0x59765fde, 0x5b8b8239, 0x599a4c12, 0x5b68596d, 0x59be2a74, 0x5b452288,
    0x59e1faff, 0x5b21dd90, 0x5a05bdae, 0x5afe8a8b, 0x5a29727b, 0x5adb297d,
    0x5a4d1960, 0x5ab7ba6c, 0x5a70b258, 0x5a943d5e,
};

const int32_t DecoderVorbisNative::INVSQ_LOOKUP_I[64 + 1] = {
    92682, 91966, 91267, 90583, 89915, 89261, 88621, 87995, 87381, 86781, 86192,
    85616, 85051, 84497, 83953, 83420, 82897, 82384, 81880, 81385, 80899, 80422,
    79953, 79492, 79039, 78594, 78156, 77726, 77302, 76885, 76475, 76072, 75674,
    75283, 74898, 74519, 74146, 73778, 73415, 73058, 72706, 72359, 72016, 71679,
    71347, 71019, 70695, 70376, 70061, 69750, 69444, 69141, 68842, 68548, 68256,
    67969, 67685, 67405, 67128, 66855, 66585, 66318, 66054, 65794, 65536,
};

const int32_t DecoderVorbisNative::INVSQ_LOOKUP_IDel[64] = {
    716, 699, 684, 668, 654, 640, 626, 614, 600, 589, 576, 565, 554,
    544, 533, 523, 513, 504, 495, 486, 477, 469, 461, 453, 445, 438,
    430, 424, 417, 410, 403, 398, 391, 385, 379, 373, 368, 363, 357,
    352, 347, 343, 337, 332, 328, 324, 319, 315, 311, 306, 303, 299,
    294, 292, 287, 284, 280, 277, 273, 270, 267, 264, 260, 258,
};

const int32_t DecoderVorbisNative::COS_LOOKUP_I[COS_LOOKUP_I_SZ + 1] = {
    16384,  16379,  16364,  16340,  16305,  16261,  16207,  16143,  16069,
    15986,  15893,  15791,  15679,  15557,  15426,  15286,  15137,  14978,
    14811,  14635,  14449,  14256,  14053,  13842,  13623,  13395,  13160,
    12916,  12665,  12406,  12140,  11866,  11585,  11297,  11003,  10702,
    10394,  10080,  9760,   9434,   9102,   8765,   8423,   8076,   7723,
    7366,   7005,   6639,   6270,   5897,   5520,   5139,   4756,   4370,
    3981,   3590,   3196,   2801,   2404,   2006,   1606,   1205,   804,
    402,    0,      -401,   -803,   -1204,  -1605,  -2005,  -2403,  -2800,
    -3195,  -3589,  -3980,  -4369,  -4755,  -5138,  -5519,  -5896,  -6269,
    -6638,  -7004,  -7365,  -7722,  -8075,  -8422,  -8764,  -9101,  -9433,
    -9759,  -10079, -10393, -10701, -11002, -11296, -11584, -11865, -12139,
    -12405, -12664, -12915, -13159, -13394, -13622, -13841, -14052, -14255,
    -14448, -14634, -14810, -14977, -15136, -15285, -15425, -15556, -15678,
    -15790, -15892, -15985, -16068, -16142, -16206, -16260, -16304, -16339,
    -16363, -16378, -16383,
};
