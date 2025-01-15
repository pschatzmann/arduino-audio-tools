/*
 * opus_decoder.cpp
 * based on Xiph.Org Foundation celt decoder
 *
 *  Created on: 26.01.2023
 *  Updated on: 21.12.2024
 */
//----------------------------------------------------------------------------------------------------------------------
//                                     O G G / O P U S     I M P L.
//----------------------------------------------------------------------------------------------------------------------
#include "opus_decoder.h"
#include "celt.h"
#include "silk.h"
#include "Arduino.h"
#include <vector>

#define __malloc_heap_psram(size) \
    heap_caps_malloc_prefer(size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)
#define __calloc_heap_psram(ch, size) \
    heap_caps_calloc_prefer(ch, size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)

// global vars
const uint32_t CELT_SET_END_BAND_REQUEST   = 10012;
const uint32_t CELT_SET_START_BAND_REQUEST = 10010;
const uint32_t CELT_SET_SIGNALLING_REQUEST = 10016;
const uint32_t CELT_GET_AND_CLEAR_ERROR_REQUEST = 10007;

enum {OPUS_BANDWIDTH_NARROWBAND = 1101,    OPUS_BANDWIDTH_MEDIUMBAND = 1102, OPUS_BANDWIDTH_WIDEBAND = 1103,
      OPUS_BANDWIDTH_SUPERWIDEBAND = 1104, OPUS_BANDWIDTH_FULLBAND = 1105};
enum {MODE_NONE = 0, MODE_SILK_ONLY = 1000, MODE_HYBRID = 1001,  MODE_CELT_ONLY = 1002};

bool      s_f_opusParseOgg = false;
bool      s_f_newSteamTitle = false;  // streamTitle
bool      s_f_opusNewMetadataBlockPicture = false; // new metadata block picture
bool      s_f_opusStereoFlag = false;
bool      s_f_continuedPage = false;
bool      s_f_firstPage = false;
bool      s_f_lastPage = false;
bool      s_f_nextChunk = false;

uint8_t   s_opusChannels = 0;
uint16_t  s_mode = 0;
uint8_t   s_opusCountCode =  0;
uint8_t   s_opusPageNr = 0;
uint8_t   s_frameCount = 0;
uint16_t  s_opusOggHeaderSize = 0;
uint16_t  s_bandWidth = 0;
uint16_t  s_internalSampleRate = 0;
uint16_t  s_endband =0;
uint32_t  s_opusSamplerate = 0;
uint32_t  s_opusSegmentLength = 0;
uint32_t  s_opusCurrentFilePos = 0;
uint32_t  s_opusAudioDataStart = 0;
int32_t   s_opusBlockPicLen = 0;
int32_t   s_blockPicLenUntilFrameEnd = 0;
int32_t   s_opusRemainBlockPicLen = 0;
int32_t   s_opusCommentBlockSize = 0;
uint32_t  s_opusBlockPicPos = 0;
uint32_t  s_opusBlockLen = 0;
char     *s_opusChbuf = NULL;
int32_t   s_opusValidSamples = 0;

uint16_t *s_opusSegmentTable;
uint8_t   s_opusSegmentTableSize = 0;
int16_t   s_opusSegmentTableRdPtr = -1;
int8_t    s_opusError = 0;
int8_t    s_prev_mode = 0;
float     s_opusCompressionRatio = 0;

std::vector <uint32_t>s_opusBlockPicItem;

bool OPUSDecoder_AllocateBuffers(){
    s_opusChbuf = (char*)__malloc_heap_psram(512);
    if(!CELTDecoder_AllocateBuffers()) {log_e("CELT not init"); return false;}
    s_opusSegmentTable = (uint16_t*)__malloc_heap_psram(256 * sizeof(uint16_t));
    if(!s_opusSegmentTable) {log_e("CELT not init"); return false;}
    CELTDecoder_ClearBuffer();
    OPUSDecoder_ClearBuffers();
    // allocate CELT buffers after OPUS head (nr of channels is needed)
    s_opusError = celt_decoder_init(2); if(s_opusError < 0) {log_e("CELT not init"); return false;}
    s_opusError = celt_decoder_ctl(CELT_SET_SIGNALLING_REQUEST,  0); if(s_opusError < 0) {log_e("CELT not init"); return false;}
    s_opusError = celt_decoder_ctl(CELT_SET_END_BAND_REQUEST,   21); if(s_opusError < 0) {log_e("CELT not init"); return false;}
    OPUSsetDefaults();

    int32_t ret = 0, silkDecSizeBytes = 0;
    (void) ret;
    (void) silkDecSizeBytes;
    silk_InitDecoder();
    //ret = silk_Get_Decoder_Size(&silkDecSizeBytes);
    // if (ret){
    //     log_e("internal error");
    // }
    // else{
    //     log_i("silkDecSizeBytes %i", silkDecSizeBytes);
    // }
    return true;
}
void OPUSDecoder_FreeBuffers(){
    if(s_opusChbuf)        {free(s_opusChbuf);        s_opusChbuf = NULL;}
    if(s_opusSegmentTable) {free(s_opusSegmentTable); s_opusSegmentTable = NULL;}
    s_frameCount = 0;
    s_opusSegmentLength = 0;
    s_opusValidSamples = 0;
    s_opusSegmentTableSize = 0;
    s_opusOggHeaderSize = 0;
    s_opusSegmentTableRdPtr = -1;
    s_opusCountCode = 0;
    CELTDecoder_FreeBuffers();
}
void OPUSDecoder_ClearBuffers(){
    if(s_opusChbuf)        memset(s_opusChbuf, 0, 512);
    if(s_opusSegmentTable) memset(s_opusSegmentTable, 0, 256 * sizeof(int16_t));
    s_frameCount = 0;
    s_opusSegmentLength = 0;
    s_opusValidSamples = 0;
    s_opusSegmentTableSize = 0;
    s_opusOggHeaderSize = 0;
    s_opusSegmentTableRdPtr = -1;
    s_opusCountCode = 0;
}
void OPUSsetDefaults(){
    s_f_opusParseOgg = false;
    s_f_newSteamTitle = false;  // streamTitle
    s_f_opusNewMetadataBlockPicture = false;
    s_f_opusStereoFlag = false;
    s_opusChannels = 0;
    s_frameCount = 0;
    s_mode = 0;
    s_opusSamplerate = 0;
    s_internalSampleRate = 0;
    s_bandWidth = 0;
    s_opusSegmentLength = 0;
    s_opusValidSamples = 0;
    s_opusSegmentTableSize = 0;
    s_opusOggHeaderSize = 0;
    s_opusSegmentTableRdPtr = -1;
    s_opusCountCode = 0;
    s_opusBlockPicPos = 0;
    s_opusCurrentFilePos = 0;
    s_opusAudioDataStart = 0;
    s_opusBlockPicLen = 0;
    s_opusCommentBlockSize = 0;
    s_opusRemainBlockPicLen = 0;
    s_blockPicLenUntilFrameEnd = 0;
    s_opusBlockLen = 0;
    s_opusPageNr = 0;
    s_opusError = 0;
    s_endband = 0;
    s_prev_mode = 0;
    s_opusBlockPicItem.clear(); s_opusBlockPicItem.shrink_to_fit();
}

//----------------------------------------------------------------------------------------------------------------------

int32_t OPUSDecode(uint8_t* inbuf, int32_t* bytesLeft, int16_t* outbuf) {

    int32_t ret = ERR_OPUS_NONE;
    int32_t segmLen = 0;

    if(s_opusCommentBlockSize) {
        if(s_opusCommentBlockSize > 8192) {
            s_opusRemainBlockPicLen -= 8192;
            *bytesLeft -= 8192;
            s_opusCurrentFilePos += 8192;
            s_opusCommentBlockSize -= 8192;
        }
        else {
            s_opusRemainBlockPicLen -= s_opusCommentBlockSize;
            *bytesLeft -= s_opusCommentBlockSize;
            s_opusCurrentFilePos += s_opusCommentBlockSize;
            s_opusCommentBlockSize = 0;
        }
        if(s_opusRemainBlockPicLen <= 0) {
            if(s_opusBlockPicItem.size() > 0) { // get blockpic data
                // log_i("---------------------------------------------------------------------------");
                // log_i("metadata blockpic found at pos %i, size %i bytes", s_vorbisBlockPicPos, s_vorbisBlockPicLen);
                // for(int32_t i = 0; i < s_vorbisBlockPicItem.size(); i += 2) { log_i("segment %02i, pos %07i, len %05i", i / 2, s_vorbisBlockPicItem[i], s_vorbisBlockPicItem[i + 1]); }
                // log_i("---------------------------------------------------------------------------");
                s_f_opusNewMetadataBlockPicture = true;
            }
        }
        return OPUS_PARSE_OGG_DONE;
    }

    if(s_frameCount > 0) return opusDecodePage3(inbuf, bytesLeft, segmLen, outbuf); // decode audio, next part

    if(!s_opusSegmentTableSize) {
        s_f_opusParseOgg = false;
        s_opusCountCode = 0;
        ret = OPUSparseOGG(inbuf, bytesLeft);
        if(ret != ERR_OPUS_NONE) return ret; // error
        inbuf += s_opusOggHeaderSize;
    }

    if(s_opusSegmentTableSize > 0) {
        s_opusSegmentTableRdPtr++;
        s_opusSegmentTableSize--;
        segmLen = s_opusSegmentTable[s_opusSegmentTableRdPtr];
    }

    if(s_opusPageNr == 0) { // OpusHead
        ret = opusDecodePage0(inbuf, bytesLeft, segmLen);
    }
    else if(s_opusPageNr == 1) { // OpusComment
        ret = parseOpusComment(inbuf, segmLen);
        if(ret == 0) log_e("OpusCommemtPage not found");
        s_opusRemainBlockPicLen = s_opusBlockPicLen;
        *bytesLeft -= (segmLen - s_blockPicLenUntilFrameEnd);
        s_opusCommentBlockSize = s_blockPicLenUntilFrameEnd;
        s_opusPageNr++;
        ret = OPUS_PARSE_OGG_DONE;
    }
    else if(s_opusPageNr == 2) { // OpusComment Subsequent Pages
        s_opusCommentBlockSize = segmLen;
        if(s_opusRemainBlockPicLen <= segmLen) s_opusPageNr++;
        ;
        ret = OPUS_PARSE_OGG_DONE;
    }
    else if(s_opusPageNr == 3) {
        ret = opusDecodePage3(inbuf, bytesLeft, segmLen, outbuf); // decode audio
    }
    else { ; }

    if(s_opusSegmentTableSize == 0) {
        s_opusSegmentTableRdPtr = -1; // back to the parking position
    }
    return ret;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
int32_t opusDecodePage0(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength){
    int32_t ret = 0;
    ret = parseOpusHead(inbuf, segmentLength);
    *bytesLeft           -= segmentLength;
    s_opusCurrentFilePos += segmentLength;
    if(ret == 1){ s_opusPageNr++;}
    if(ret == 0){ log_e("OpusHead not found"); }
    if(ret < 0) return ret;
    return OPUS_PARSE_OGG_DONE;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------
int32_t opusDecodePage3(uint8_t* inbuf, int32_t* bytesLeft, uint32_t segmentLength, int16_t *outbuf){

    if(s_opusAudioDataStart == 0){
        s_opusAudioDataStart = s_opusCurrentFilePos;
    }


    s_endband = 21;
    static int8_t configNr = 0;
    static uint16_t samplesPerFrame = 0;

    int32_t ret = 0;

    if(s_frameCount > 0) goto FramePacking; // more than one frame in the packet

    configNr = parseOpusTOC(inbuf[0]);
    if(configNr < 0) {log_e("something went wrong");  return configNr;} // SILK or Hybrid mode

    switch(configNr){
        case  0 ... 3:  s_endband  = 0; // OPUS_BANDWIDTH_SILK_NARROWBAND
                        s_mode = MODE_SILK_ONLY;
                        s_bandWidth = OPUS_BANDWIDTH_NARROWBAND;
                        s_internalSampleRate = 8000;
                        break;
        case  4 ... 7:  s_endband  = 0; // OPUS_BANDWIDTH_SILK_MEDIUMBAND
                        s_mode = MODE_SILK_ONLY;
                        s_bandWidth = OPUS_BANDWIDTH_MEDIUMBAND;
                        s_internalSampleRate = 12000;
                        break;
        case  8 ... 11: s_endband  = 0; // OPUS_BANDWIDTH_SILK_WIDEBAND
                        s_mode = MODE_SILK_ONLY;
                        s_bandWidth = OPUS_BANDWIDTH_WIDEBAND;
                        s_internalSampleRate = 16000;
                        break;
        case 12 ... 13: s_endband  = 0; // OPUS_BANDWIDTH_HYBRID_SUPERWIDEBAND
                        s_mode = MODE_HYBRID;
                        s_bandWidth = OPUS_BANDWIDTH_SUPERWIDEBAND;
                        break;
        case 14 ... 15: s_endband  = 0; // OPUS_BANDWIDTH_HYBRID_FULLBAND
                        s_mode = MODE_HYBRID;
                        s_bandWidth = OPUS_BANDWIDTH_FULLBAND;
                        break;
        case 16 ... 19: s_endband = 13; // OPUS_BANDWIDTH_CELT_NARROWBAND
                        s_mode = MODE_CELT_ONLY;
                        s_bandWidth = OPUS_BANDWIDTH_NARROWBAND;
                        break;
        case 20 ... 23: s_endband = 17; // OPUS_BANDWIDTH_CELT_WIDEBAND
                        s_mode = MODE_CELT_ONLY;
                        s_bandWidth = OPUS_BANDWIDTH_WIDEBAND;
                        break;
        case 24 ... 27: s_endband = 19; // OPUS_BANDWIDTH_CELT_SUPERWIDEBAND
                        s_mode = MODE_CELT_ONLY;
                        s_bandWidth = OPUS_BANDWIDTH_SUPERWIDEBAND;
                        break;
        case 28 ... 31: s_endband = 21; // OPUS_BANDWIDTH_CELT_FULLBAND
                        s_mode = MODE_CELT_ONLY;
                        s_bandWidth = OPUS_BANDWIDTH_FULLBAND;
                        break;
        default:        log_e("unknown bandwidth, configNr is: %d", configNr);
                        s_endband = 21; // assume OPUS_BANDWIDTH_FULLBAND
                        break;
    }

//    celt_decoder_ctl(CELT_SET_START_BAND_REQUEST, s_endband);
    if (s_mode == MODE_CELT_ONLY){
        celt_decoder_ctl(CELT_SET_END_BAND_REQUEST, s_endband);
    }
    else if(s_mode == MODE_SILK_ONLY){
        // silk_InitDecoder();
    }

    samplesPerFrame = opus_packet_get_samples_per_frame(inbuf, /*s_opusSamplerate*/ 48000);

FramePacking:            // https://www.tech-invite.com/y65/tinv-ietf-rfc-6716-2.html   3.2. Frame Packing
//log_i("s_opusCountCode %i, configNr %i", s_opusCountCode, configNr);

    switch(s_opusCountCode){
        case 0:  // Code 0: One Frame in the Packet
            ret = opus_FramePacking_Code0(inbuf, bytesLeft, outbuf, segmentLength, samplesPerFrame);
            break;
        case 1:  // Code 1: Two Frames in the Packet, Each with Equal Compressed Size
            ret = opus_FramePacking_Code1(inbuf, bytesLeft, outbuf, segmentLength, samplesPerFrame, &s_frameCount);
            break;
        case 2:  // Code 2: Two Frames in the Packet, with Different Compressed Sizes
            ret = opus_FramePacking_Code2(inbuf, bytesLeft, outbuf, segmentLength, samplesPerFrame, &s_frameCount);
            break;
        case 3: // Code 3: A Signaled Number of Frames in the Packet
            ret = opus_FramePacking_Code3(inbuf, bytesLeft, outbuf, segmentLength, samplesPerFrame, &s_frameCount);
            break;
        default:
            log_e("unknown countCode %i", s_opusCountCode);
            break;
    }
    return ret;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------
int32_t opus_decode_frame(uint8_t *inbuf, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame) {

    int32_t   ret = 0;

    if (s_mode == MODE_CELT_ONLY){
        celt_decoder_ctl(CELT_SET_END_BAND_REQUEST, s_endband);
        ec_dec_init((uint8_t *)inbuf, packetLen);
        ret = celt_decode_with_ec((int16_t*)outbuf, samplesPerFrame);
    }

    if(s_mode == MODE_SILK_ONLY) {
        int decodedSamples = 0;
        int32_t silk_frame_size;
        uint16_t payloadSize_ms = max(10, 1000 * samplesPerFrame / 48000);
        if(s_bandWidth == OPUS_BANDWIDTH_NARROWBAND) { s_internalSampleRate = 8000; }
        else if(s_bandWidth == OPUS_BANDWIDTH_MEDIUMBAND) { s_internalSampleRate = 12000; }
        else if(s_bandWidth == OPUS_BANDWIDTH_WIDEBAND) { s_internalSampleRate = 16000; }
        else { s_internalSampleRate = 16000; }
        ec_dec_init((uint8_t *)inbuf, packetLen);
        uint8_t APIchannels = 2;
        silk_setRawParams(s_opusChannels, APIchannels, payloadSize_ms, s_internalSampleRate, 48000);
        do{
            /* Call SILK decoder */
            int lost_flag = 0;
            int first_frame = decodedSamples == 0;
            int silk_ret = silk_Decode(lost_flag, first_frame, (int16_t*)outbuf + decodedSamples, &silk_frame_size);
            if(silk_ret)log_w("silk_ret %i", silk_ret);
            decodedSamples += silk_frame_size;
        } while(decodedSamples < samplesPerFrame);
        ret = decodedSamples;
    }

    if(s_mode == MODE_HYBRID){
        log_w("Hybrid mode not yet supported");
        return samplesPerFrame;
        int decodedSamples = 0;
         int start_band = 0;
        int32_t silk_frame_size;
        int F2_5, F5, F10, F20;
        F20 = packetLen / 50;
        F10 = F20 >> 1;
        F5 = F10 >> 1;
        F2_5 = F5 >> 1;
        if(packetLen < F2_5) { return ERR_OPUS_BUFFER_TOO_SMALL; }
        ec_dec_init((uint8_t *)inbuf, packetLen);
        s_internalSampleRate = 16000;
        uint8_t APIchannels = 2;
        uint16_t payloadSize_ms = max(10, 1000 * samplesPerFrame / 48000);
        int lost_flag = 0;
        int first_frame = decodedSamples == 0;
        silk_setRawParams(s_opusChannels, APIchannels, payloadSize_ms, s_internalSampleRate, 48000);
        silk_Decode(lost_flag, first_frame, (int16_t*)outbuf + decodedSamples, &silk_frame_size);
        if(s_bandWidth) {
            s_endband = 21;
            switch(s_bandWidth) {
                case OPUS_BANDWIDTH_NARROWBAND: s_endband = 13; break;
                case OPUS_BANDWIDTH_MEDIUMBAND:
                case OPUS_BANDWIDTH_WIDEBAND: s_endband = 17; break;
                case OPUS_BANDWIDTH_SUPERWIDEBAND: s_endband = 19; break;
                case OPUS_BANDWIDTH_FULLBAND: s_endband = 21; break;
                default: break;
            }
        }
        start_band = 17;
        celt_decoder_ctl(CELT_SET_START_BAND_REQUEST, start_band);
    //    celt_decoder_ctl(CELT_SET_END_BAND_REQUEST, s_endband);
        ret = celt_decode_with_ec((int16_t*)outbuf, samplesPerFrame);
    }
    return ret;
}
//----------------------------------------------------------------------------------------------------------------------------------------------------
int8_t opus_FramePacking_Code0(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame){

/*  Code 0: One Frame in the Packet

    For code 0 packets, the TOC byte is immediately followed by N-1 bytes
    of compressed data for a single frame (where N is the size of the
    packet), as illustrated
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      | config  |s|0|0|                                               |
      +-+-+-+-+-+-+-+-+                                               |
      |                    Compressed frame 1 (N-1 bytes)...          :
      :                                                               |
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
    int32_t ret = 0;
    *bytesLeft -= packetLen;
    s_opusCurrentFilePos += packetLen;
    packetLen--;
    inbuf++;
    ret = opus_decode_frame(inbuf, outbuf, packetLen, samplesPerFrame);
// log_w("code 0, ret %i", ret);
    if(ret < 0){
        return ret; // decode err
    }
    s_opusValidSamples = ret;
    return ERR_OPUS_NONE;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
int8_t opus_FramePacking_Code1(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount){

/*  Code 1: Two Frames in the Packet, Each with Equal Compressed Size

   For code 1 packets, the TOC byte is immediately followed by the (N-1)/2 bytes [where N is the size of the packet] of compressed
   data for the first frame, followed by (N-1)/2 bytes of compressed data for the second frame, as illustrated. The number of payload bytes
   available for compressed data, N-1, MUST be even for all code 1 packets.
      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     | config  |s|0|1|                                               |
     +-+-+-+-+-+-+-+-+                                               :
     |             Compressed frame 1 ((N-1)/2 bytes)...             |
     :                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                               |                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               :
     |             Compressed frame 2 ((N-1)/2 bytes)...             |
     :                                               +-+-+-+-+-+-+-+-+
     |                                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
    int32_t ret = 0;
    static uint16_t c1fs = 0;
    if(*frameCount == 0){
        packetLen--;
        inbuf++;
        *bytesLeft -= 1;
        s_opusCurrentFilePos += 1;
        c1fs = packetLen / 2;
        // log_w("OPUS countCode 1 len %i, c1fs %i", len, c1fs);
        *frameCount = 2;
    }
    if(*frameCount > 0){
        ret = opus_decode_frame(inbuf, outbuf, c1fs, samplesPerFrame);
        // log_w("code 1, ret %i", ret);
        if(ret < 0){
            *frameCount = 0;
            return ret;  // decode err
        }
        s_opusValidSamples = ret;
        *bytesLeft -= c1fs;
        s_opusCurrentFilePos += c1fs;
    }
    *frameCount -= 1;
    return ERR_OPUS_NONE;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
int8_t opus_FramePacking_Code2(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount){

/*  Code 2: Two Frames in the Packet, with Different Compressed Sizes

   For code 2 packets, the TOC byte is followed by a one- or two-byte sequence indicating the length of the first frame (marked N1 in the Figure),
   followed by N1 bytes of compressed data for the first frame.  The remaining N-N1-2 or N-N1-3 bytes are the compressed data for the second frame.
   This is illustrated in the Figure.  A code 2 packet MUST contain enough bytes to represent a valid length.  For example, a 1-byte code 2 packet
   is always invalid, and a 2-byte code 2 packet whose second byte is in the range 252...255 is also invalid. The length of the first frame, N1,
   MUST also be no larger than the size of the payload remaining after decoding that length for all code 2 packets. This makes, for example,
   a 2-byte code 2 packet with a second byte in the range 1...251 invalid as well (the only valid 2-byte code 2 packet is one where the length of
   both frames is zero).
   compute N1:  o  1...251: Length of the frame in bytes
                o  252...255: A second byte is needed.  The total length is (second_byte*4)+first_byte

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     | config  |s|1|0| N1 (1-2 bytes):                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               :
     |               Compressed frame 1 (N1 bytes)...                |
     :                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                               |                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
     |                     Compressed frame 2...                     :
     :                                                               |
     |                                                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
    int32_t ret = 0;
    static uint16_t firstFrameLength = 0;
    static uint16_t secondFrameLength = 0;

    if(*frameCount == 0){
        uint8_t b1 = inbuf[1];
        uint8_t b2 = inbuf[2];
        if(b1 < 252){
            firstFrameLength = b1;
            packetLen -= 2;
            *bytesLeft -= 2;
            s_opusCurrentFilePos += 2;
            inbuf += 2;
        }
        else{
            firstFrameLength = b1 + (b2 * 4);
            packetLen -= 3;
            *bytesLeft -= 3;
            s_opusCurrentFilePos += 3;
            inbuf += 3;
        }
        secondFrameLength = packetLen - firstFrameLength;
        *frameCount = 2;
    }
    if(*frameCount == 2){
        ret = opus_decode_frame(inbuf, outbuf, firstFrameLength, samplesPerFrame);
        // log_w("code 2, ret %i", ret);
        if(ret < 0){
            *frameCount = 0;
            return ret;  // decode err
        }
        s_opusValidSamples = ret;
        *bytesLeft -= firstFrameLength;
        s_opusCurrentFilePos += firstFrameLength;
    }
    if(*frameCount == 1){
        ret = opus_decode_frame(inbuf, outbuf, secondFrameLength, samplesPerFrame);
        // log_w("code 2, ret %i", ret);
        if(ret < 0){
            *frameCount = 0;
            return ret;  // decode err
        }
        s_opusValidSamples = ret;
        *bytesLeft -= secondFrameLength;
        s_opusCurrentFilePos += secondFrameLength;
    }
    *frameCount -= 1;
    return ERR_OPUS_NONE;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
int8_t opus_FramePacking_Code3(uint8_t *inbuf, int32_t *bytesLeft, int16_t *outbuf, int32_t packetLen, uint16_t samplesPerFrame, uint8_t* frameCount){

/*  Code 3: A Signaled Number of Frames in the Packet

   Code 3 packets signal the number of frames, as well as additional padding, called "Opus padding" to indicate that this padding is added
   at the Opus layer rather than at the transport layer.  Code 3 packets MUST have at least 2 bytes [R6,R7].  The TOC byte is followed by a
   byte encoding the number of frames in the packet in bits 2 to 7 (marked "M" in the Figure )           0
                                                                                                         0 1 2 3 4 5 6 7
                                                                                                        +-+-+-+-+-+-+-+-+
                                                                                                        |v|p|     M     |
                                                                                                        +-+-+-+-+-+-+-+-+
   with bit 1 indicating whether or not Opus
   padding is inserted (marked "p" in Figure 5), and bit 0 indicating VBR (marked "v" in Figure). M MUST NOT be zero, and the audio
   duration contained within a packet MUST NOT exceed 120 ms. This limits the maximum frame count for any frame size to 48 (for 2.5 ms
   frames), with lower limits for longer frame sizes. The Figure below illustrates the layout of the frame count byte.
   When Opus padding is used, the number of bytes of padding is encoded in the bytes following the frame count byte.  Values from 0...254
   indicate that 0...254 bytes of padding are included, in addition to the byte(s) used to indicate the size of the padding.  If the value
   is 255, then the size of the additional padding is 254 bytes, plus the padding value encoded in the next byte.

                           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   Padding Length 254      | 253 |           253 x 0x00                                     :
                           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   Padding Length 255      | 254 |           254 x 0x00                                     :
                           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   Padding Length 256      | 255 |  0  |     254 x 0x00                                     :
                           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   There MUST be at least one more byte in the packet in this case [R6,R7]. The additional padding bytes appear at the end of the packet and MUST
   be set to zero by the encoder to avoid creating a covert channel. The decoder MUST accept any value for the padding bytes, however.
   Although this encoding provides multiple ways to indicate a given number of padding bytes, each uses a different number of bytes to
   indicate the padding size and thus will increase the total packet size by a different amount.  For example, to add 255 bytes to a
   packet, set the padding bit, p, to 1, insert a single byte after the frame count byte with a value of 254, and append 254 padding bytes
   with the value zero to the end of the packet.  To add 256 bytes to a packet, set the padding bit to 1, insert two bytes after the frame
   count byte with the values 255 and 0, respectively, and append 254 padding bytes with the value zero to the end of the packet.  By using
   the value 255 multiple times, it is possible to create a packet of any specific, desired size.  Let P be the number of header bytes used
   to indicate the padding size plus the number of padding bytes themselves (i.e., P is the total number of bytes added to the
   packet). Then, P MUST be no more than N-2. In the CBR case, let R=N-2-P be the number of bytes remaining in the packet after subtracting the
   (optional) padding. Then, the compressed length of each frame in bytes is equal to R/M.  The value R MUST be a non-negative integer multiple
   of M. The compressed data for all M frames follows, each of size R/M bytes, as illustrated in the Figure below.
      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     | config  |s|1|1|0|p|     M     |  Padding length (Optional)    :
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                                                               |
     :               Compressed frame 1 (R/M bytes)...               :
     |                                                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                                                               |
     :               Compressed frame 2 (R/M bytes)...               :
     |                                                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                                                               |
     :                              ...                              :
     |                                                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                                                               |
     :               Compressed frame M (R/M bytes)...               :
     |                                                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     :                  Opus Padding (Optional)...                   |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   In the VBR case, the (optional) padding length is followed by M-1 frame lengths (indicated by "N1" to "N[M-1]" in Figure 7), each encoded in a
   one- or two-byte sequence as described above. The packet MUST contain enough data for the M-1 lengths after removing the (optional) padding,
   and the sum of these lengths MUST be no larger than the number of bytes remaining in the packet after decoding them. The compressed data for
   all M frames follows, each frame consisting of the indicated number of bytes, with the final frame consuming any remaining bytes before the final
   padding, as illustrated in the Figure below. The number of header bytes (TOC byte, frame count byte, padding length bytes, and frame length bytes),
   plus the signaled length of the first M-1 frames themselves, plus the signaled length of the padding MUST be no larger than N, the total
   size of the packet.
      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     | config  |s|1|1|1|p|     M     | Padding length (Optional)     :
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     : N1 (1-2 bytes): N2 (1-2 bytes):     ...       :     N[M-1]    |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                                                               |
     :               Compressed frame 1 (N1 bytes)...                :
     |                                                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                                                               |
     :               Compressed frame 2 (N2 bytes)...                :
     |                                                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                                                               |
     :                              ...                              :
     |                                                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                                                               |
     :                     Compressed frame M...                     :
     |                                                               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     :                  Opus Padding (Optional)...                   |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

*/
    static bool    firstCall = true;
    static bool    v = false; // VBR indicator
    static bool    p = false; // padding exists
    static int16_t fs = 0;    // frame size
    static uint8_t M = 0;     // nr of frames
    static int32_t spf = 0;   // static samples per frame
    static int32_t paddingLength = 0;
    uint32_t       idx = 0;   // includes TOC byte
    int32_t        ret = 0;
    int32_t        remainingBytes = 0;
    uint16_t       vfs[48] = {0}; // variable frame size

    if (firstCall) {
        firstCall = false;
        s_opusCurrentFilePos += packetLen;
        paddingLength = 0;
        idx = 1; // skip TOC byte
        spf = samplesPerFrame;
        if (inbuf[idx] & 0b10000000) v = true; // VBR indicator
        if (inbuf[idx] & 0b01000000) p = true; // padding bit
        M = inbuf[idx] & 0b00111111;           // framecount
        *frameCount = M;
        idx++;
        if (p) {
            while (inbuf[idx] == 255) { paddingLength += 255, idx++; }
            paddingLength += inbuf[idx];
            idx++;
        }
        if (v && M) { // variable frame size
            uint8_t m = 0;
            while(m <  M) {
                vfs[m] = inbuf[idx];
                if(vfs[m] == 255) {vfs[m] = 255; idx++;}
                idx++;
                m++;
                // log_e("vfs[m] %i", vfs[m]);
            }
        }
        remainingBytes = packetLen - paddingLength - idx;
        if (remainingBytes < 0) {
            // log_e("fs %i", fs);
            *bytesLeft -= packetLen;
            *frameCount = 0;
            firstCall = true;
            return ERR_OPUS_NONE;
        }
        if(!v){
            fs = remainingBytes / M;
        }
        if (fs > remainingBytes) {
            *bytesLeft -= packetLen;
            *frameCount = 0;
            firstCall = true;
            return ERR_OPUS_NONE;
        }
        //  log_w("remainingBytes %i, packetLen %i, paddingLength %i, idx %i, fs %i, frameCount %i", remainingBytes, packetLen, paddingLength, idx, fs, *frameCount);
        *bytesLeft -= idx;
    }
    if(*frameCount > 0){
        if(v){ret = opus_decode_frame(inbuf + idx, outbuf, vfs[M - (*frameCount)], spf); *bytesLeft -= vfs[M - (*frameCount)]; /* log_e("code 3, vfs[M - (*frameCount)], spf) %i",  vfs[M - (*frameCount)], spf); */ }
        else{ ret = opus_decode_frame(inbuf + idx, outbuf, fs, spf); *bytesLeft -= fs; /* log_e("code 3, fs, spf) %i", fs, spf); */ }
        // log_w("code 3, ret %i", ret);
        *frameCount -= 1;
        s_opusValidSamples = ret;
        if(*frameCount > 0) return OPUS_CONTINUE;
    }
    *bytesLeft -= paddingLength;
    *frameCount = 0;
    s_opusValidSamples = samplesPerFrame;
    firstCall = true;
    return ERR_OPUS_NONE;
}

//----------------------------------------------------------------------------------------------------------------------

int32_t opus_packet_get_samples_per_frame(const uint8_t *data, int32_t Fs) {
    int32_t audiosize;
    if ((data[0] & 0x80) == 0x080) {
        audiosize = ((data[0] >> 3) & 0x03);
        audiosize = (Fs << audiosize) / 400;
    } else if ((data[0] & 0x60) == 0x60) {
        audiosize = (data[0] & 0x08) ? Fs / 50 : Fs / 100;
    } else {
        audiosize = ((data[0] >> 3) & 0x3);
        if (audiosize == 3){
            audiosize = Fs * 60 / 1000;
        }
        else{
            audiosize = (Fs << audiosize) / 100;
        }
    }
    return audiosize;
}
//----------------------------------------------------------------------------------------------------------------------

uint8_t OPUSGetChannels(){
    return s_opusChannels;
}
uint32_t OPUSGetSampRate(){
    return 48000;
}
uint8_t OPUSGetBitsPerSample(){
    return 16;
}
uint32_t OPUSGetBitRate(){
    if(s_opusCompressionRatio != 0){
        return (16 * 2 * 48000) / s_opusCompressionRatio;  //bitsPerSample * channel* SampleRate/CompressionRatio
    }
    else return 0;
}
uint16_t OPUSGetOutputSamps(){
    return s_opusValidSamples; // 1024
}
uint32_t OPUSGetAudioDataStart(){
    return s_opusAudioDataStart;
}
char* OPUSgetStreamTitle(){
    if(s_f_newSteamTitle){
        s_f_newSteamTitle = false;
        return s_opusChbuf;
    }
    return NULL;
}
vector<uint32_t> OPUSgetMetadataBlockPicture(){
    if(s_f_opusNewMetadataBlockPicture){
        s_f_opusNewMetadataBlockPicture = false;
        return s_opusBlockPicItem;
    }
    if(s_opusBlockPicItem.size() > 0){
        s_opusBlockPicItem.clear();
        s_opusBlockPicItem.shrink_to_fit();
    }
    return s_opusBlockPicItem;
}

//----------------------------------------------------------------------------------------------------------------------
int8_t parseOpusTOC(uint8_t TOC_Byte){  // https://www.rfc-editor.org/rfc/rfc6716  page 16 ff

    uint8_t configNr = 0;
    uint8_t s = 0;              // stereo flag
    uint8_t c = 0; (void)c;     // count code

    configNr = (TOC_Byte & 0b11111000) >> 3;
    s        = (TOC_Byte & 0b00000100) >> 2;
    c        = (TOC_Byte & 0b00000011);

    /*  Configuration       Mode  Bandwidth            FrameSizes         Audio Bandwidth   Sample Rate (Effective)

        configNr  0 ...  3  SILK     NB (narrow band)      10, 20, 40, 60ms   4 kHz              8 kHz
        configNr  4 ...  7  SILK     MB (medium band)      10, 20, 40, 60ms   6 kHz             12 kHz
        configNr  8 ... 11  SILK     WB (wide band)        10, 20, 40, 60ms   8 kHz             16 kHz
        configNr 12 ... 13  HYBRID  SWB (super wideband)   10, 20ms          12 kHz (*)         24 kHz
        configNr 14 ... 15  HYBRID   FB (full band)        10, 20ms          20 kHz (*)         48 kHz
        configNr 16 ... 19  CELT     NB (narrow band)      2.5, 5, 10, 20ms   4 kHz              8 kHz
        configNr 20 ... 23  CELT     WB (wide band)        2.5, 5, 10, 20ms   8 kHz             16 kHz
        configNr 24 ... 27  CELT    SWB (super wideband)   2.5, 5, 10, 20ms   12 kHz            24 kHz
        configNr 28 ... 31  CELT     FB (full band)        2.5, 5, 10, 20ms   20 kHz (*)        48 kHz     <-------

        (*) Although the sampling theorem allows a bandwidth as large as half the sampling rate, Opus never codes
        audio above 20 kHz, as that is the generally accepted upper limit of human hearing.

        s = 0: mono 1: stereo

        c = 0: 1 frame in the packet
        c = 1: 2 frames in the packet, each with equal compressed size
        c = 2: 2 frames in the packet, with different compressed sizes
        c = 3: an arbitrary number of frames in the packet
    */
    s_opusCountCode = c;
    s_f_opusStereoFlag = s;

    // if(configNr < 12) return ERR_OPUS_SILK_MODE_UNSUPPORTED;
    // if(configNr < 16) return ERR_OPUS_HYBRID_MODE_UNSUPPORTED;
    // if(configNr < 20) return ERR_OPUS_NARROW_BAND_UNSUPPORTED;
    // if(configNr < 24) return ERR_OPUS_WIDE_BAND_UNSUPPORTED;
    // if(configNr < 28) return ERR_OPUS_SUPER_WIDE_BAND_UNSUPPORTED;

    return configNr;
}
//----------------------------------------------------------------------------------------------------------------------
int32_t parseOpusComment(uint8_t *inbuf, int32_t nBytes){      // reference https://exiftool.org/TagNames/Vorbis.html#Comments
                                                       // reference https://www.rfc-editor.org/rfc/rfc7845#section-5
    int32_t idx = OPUS_specialIndexOf(inbuf, "OpusTags", 10);
    if(idx != 0) return 0; // is not OpusTags

    char* artist = NULL;
    char* title  = NULL;

    uint16_t pos = 8;
             nBytes -= 8;
    uint32_t vendorLength       = *(inbuf + 11) << 24; // lengt of vendor string, e.g. Lavf58.65.101
             vendorLength      += *(inbuf + 10) << 16;
             vendorLength      += *(inbuf +  9) << 8;
             vendorLength      += *(inbuf +  8);
    pos += vendorLength + 4;
    nBytes -= (vendorLength + 4);
    uint32_t commentListLength  = *(inbuf + 3 + pos) << 24; // nr. of comment entries
             commentListLength += *(inbuf + 2 + pos) << 16;
             commentListLength += *(inbuf + 1 + pos) << 8;
             commentListLength += *(inbuf + 0 + pos);
    pos += 4;
    nBytes -= 4;
    for(int32_t i = 0; i < commentListLength; i++){
        uint32_t commentStringLen   = *(inbuf + 3 + pos) << 24;
                 commentStringLen  += *(inbuf + 2 + pos) << 16;
                 commentStringLen  += *(inbuf + 1 + pos) << 8;
                 commentStringLen  += *(inbuf + 0 + pos);
        pos += 4;
        nBytes -= 4;
        idx = OPUS_specialIndexOf(inbuf + pos, "artist=", 10);
        if(idx == -1) idx = OPUS_specialIndexOf(inbuf + pos, "ARTIST=", 10);
        if(idx == 0){ artist = strndup((const char*)(inbuf + pos + 7), commentStringLen - 7);
        }
        idx = OPUS_specialIndexOf(inbuf + pos, "title=", 10);
        if(idx == -1) idx = OPUS_specialIndexOf(inbuf + pos, "TITLE=", 10);
        if(idx == 0){ title = strndup((const char*)(inbuf + pos + 6), commentStringLen - 6);
        }
        idx = OPUS_specialIndexOf(inbuf + pos, "metadata_block_picture=", 25);
        if(idx == -1) idx = OPUS_specialIndexOf(inbuf + pos, "METADATA_BLOCK_PICTURE=", 25);
        if(idx == 0){
            s_opusBlockPicLen = commentStringLen - 23;
            s_opusCurrentFilePos += pos + 23;
            s_opusBlockPicPos += s_opusCurrentFilePos;
            s_blockPicLenUntilFrameEnd = nBytes - 23;
        //  log_i("metadata block picture found at pos %i, length %i", s_opusBlockPicPos, s_opusBlockPicLen);
            uint32_t pLen = _min(s_blockPicLenUntilFrameEnd, s_opusBlockPicLen);
            if(pLen){
                s_opusBlockPicItem.push_back(s_opusBlockPicPos);
                s_opusBlockPicItem.push_back(pLen);
            }
        }
        pos += commentStringLen;
        nBytes -= commentStringLen;
    }
    if(artist && title){
        strcpy(s_opusChbuf, artist);
        strcat(s_opusChbuf, " - ");
        strcat(s_opusChbuf, title);
        s_f_newSteamTitle = true;
    }
    else if(artist){
        strcpy(s_opusChbuf, artist);
        s_f_newSteamTitle = true;
    }
    else if(title){
        strcpy(s_opusChbuf, title);
        s_f_newSteamTitle = true;
    }
    if(artist){free(artist); artist = NULL;}
    if(title) {free(title);  title = NULL;}

    return 1;
}
//----------------------------------------------------------------------------------------------------------------------
int32_t parseOpusHead(uint8_t *inbuf, int32_t nBytes){  // reference https://wiki.xiph.org/OggOpus


    int32_t idx = OPUS_specialIndexOf(inbuf, "OpusHead", 10);
     if(idx != 0) {
        return 0; //is not OpusHead
     }
    uint8_t  version            = *(inbuf +  8); (void) version;
    uint8_t  channelCount       = *(inbuf +  9); // nr of channels
    uint16_t preSkip            = *(inbuf + 11) << 8;
             preSkip           += *(inbuf + 10);
    uint32_t sampleRate         = *(inbuf + 15) << 24;  // informational only
             sampleRate        += *(inbuf + 14) << 16;
             sampleRate        += *(inbuf + 13) << 8;
             sampleRate        += *(inbuf + 12);
    uint16_t outputGain         = *(inbuf + 17) << 8;  // Q7.8 in dB
             outputGain        += *(inbuf + 16);
    uint8_t  channelMap         = *(inbuf + 18);

    if(channelCount == 0 || channelCount >2) return ERR_OPUS_CHANNELS_OUT_OF_RANGE;
    s_opusChannels = channelCount;
//    log_e("sampleRate %i", sampleRate);
//    if(sampleRate != 48000 && sampleRate != 44100) return ERR_OPUS_INVALID_SAMPLERATE;
    s_opusSamplerate = sampleRate;
    if(channelMap > 1) return ERR_OPUS_EXTRA_CHANNELS_UNSUPPORTED;

    (void)outputGain;

    CELTDecoder_ClearBuffer();
    s_opusError = celt_decoder_init(s_opusChannels); if(s_opusError < 0) {log_e("CELT not init"); return false;}
    s_opusError = celt_decoder_ctl(CELT_SET_SIGNALLING_REQUEST,  0); if(s_opusError < 0) {log_e("CELT not init"); return false;}
    s_opusError = celt_decoder_ctl(CELT_SET_END_BAND_REQUEST,   21); if(s_opusError < 0) {log_e("CELT not init"); return false;}

    return 1;
}

//----------------------------------------------------------------------------------------------------------------------
int32_t OPUSparseOGG(uint8_t *inbuf, int32_t *bytesLeft){  // reference https://www.xiph.org/ogg/doc/rfc3533.txt

    int32_t idx = OPUS_specialIndexOf(inbuf, "OggS", 6);
    if(idx != 0) return ERR_OPUS_DECODER_ASYNC;

    int16_t segmentTableWrPtr = -1;

    uint8_t  version            = *(inbuf +  4); (void) version;
    uint8_t  headerType         = *(inbuf +  5); (void) headerType;
    uint64_t granulePosition    = (uint64_t)*(inbuf + 13) << 56;  // granule_position: an 8 Byte field containing -
             granulePosition   += (uint64_t)*(inbuf + 12) << 48;  // position information. For an audio stream, it MAY
             granulePosition   += (uint64_t)*(inbuf + 11) << 40;  // contain the total number of PCM samples encoded
             granulePosition   += (uint64_t)*(inbuf + 10) << 32;  // after including all frames finished on this page.
             granulePosition   += *(inbuf +  9) << 24;  // This is a hint for the decoder and gives it some timing
             granulePosition   += *(inbuf +  8) << 16;  // and position information. A special value of -1 (in two's
             granulePosition   += *(inbuf +  7) << 8;   // complement) indicates that no packets finish on this page.
             granulePosition   += *(inbuf +  6); (void) granulePosition;
    uint32_t bitstreamSerialNr  = *(inbuf + 17) << 24;  // bitstream_serial_number: a 4 Byte field containing the
             bitstreamSerialNr += *(inbuf + 16) << 16;  // unique serial number by which the logical bitstream
             bitstreamSerialNr += *(inbuf + 15) << 8;   // is identified.
             bitstreamSerialNr += *(inbuf + 14); (void) bitstreamSerialNr;
    uint32_t pageSequenceNr     = *(inbuf + 21) << 24;  // page_sequence_number: a 4 Byte field containing the sequence
             pageSequenceNr    += *(inbuf + 20) << 16;  // number of the page so the decoder can identify page loss
             pageSequenceNr    += *(inbuf + 19) << 8;   // This sequence number is increasing on each logical bitstream
             pageSequenceNr    += *(inbuf + 18); (void) pageSequenceNr;
    uint32_t CRCchecksum        = *(inbuf + 25) << 24;
             CRCchecksum       += *(inbuf + 24) << 16;
             CRCchecksum       += *(inbuf + 23) << 8;
             CRCchecksum       += *(inbuf + 22); (void) CRCchecksum;
    uint8_t  pageSegments       = *(inbuf + 26);        // giving the number of segment entries

    // read the segment table (contains pageSegments bytes),  1...251: Length of the frame in bytes,
    // 255: A second byte is needed.  The total length is first_byte + second byte
    s_opusSegmentLength = 0;
    segmentTableWrPtr = -1;

    for(int32_t i = 0; i < pageSegments; i++){
        int32_t n = *(inbuf + 27 + i);
        while(*(inbuf + 27 + i) == 255){
            i++;
            if(i == pageSegments) break;
            n+= *(inbuf + 27 + i);
        }
        segmentTableWrPtr++;
        s_opusSegmentTable[segmentTableWrPtr] = n;
        s_opusSegmentLength += n;
    }
    s_opusSegmentTableSize = segmentTableWrPtr + 1;
    s_opusCompressionRatio = (float)(960 * 2 * pageSegments)/s_opusSegmentLength;  // const 960 validBytes out

    s_f_continuedPage = headerType & 0x01; // set: page contains data of a packet continued from the previous page
    s_f_firstPage     = headerType & 0x02; // set: this is the first page of a logical bitstream (bos)
    s_f_lastPage      = headerType & 0x04; // set: this is the last page of a logical bitstream (eos)

//  log_i("firstPage %i, continuedPage %i, lastPage %i",s_f_firstPage, s_f_continuedPage, s_f_lastPage);

    uint16_t headerSize   = pageSegments + 27;
    *bytesLeft           -= headerSize;
    s_opusCurrentFilePos += headerSize;
    s_opusOggHeaderSize   = headerSize;

    int32_t pLen = _min((int32_t)s_opusSegmentLength, s_opusRemainBlockPicLen);
//  log_i("s_opusSegmentLength %i, s_opusRemainBlockPicLen %i", s_opusSegmentLength, s_opusRemainBlockPicLen);
    if(s_opusBlockPicLen && pLen > 0){
        s_opusBlockPicItem.push_back(s_opusCurrentFilePos);
        s_opusBlockPicItem.push_back(pLen);
    }
    return ERR_OPUS_NONE;
}

//----------------------------------------------------------------------------------------------------------------------
int32_t OPUSFindSyncWord(unsigned char *buf, int32_t nBytes){
    // assume we have a ogg wrapper
    int32_t idx = OPUS_specialIndexOf(buf, "OggS", nBytes);
    if(idx >= 0){ // Magic Word found
    //    log_i("OggS found at %i", idx);
        s_f_opusParseOgg = true;
        return idx;
    }
    log_i("find sync");
    s_f_opusParseOgg = false;
    return ERR_OPUS_OGG_SYNC_NOT_FOUND;
}
//----------------------------------------------------------------------------------------------------------------------
int32_t OPUS_specialIndexOf(uint8_t* base, const char* str, int32_t baselen, bool exact){
    int32_t result = -1;  // seek for str in buffer or in header up to baselen, not nullterninated
    if (strlen(str) > baselen) return -1; // if exact == true seekstr in buffer must have "\0" at the end
    for (int32_t i = 0; i < baselen - strlen(str); i++){
        result = i;
        for (int32_t j = 0; j < strlen(str) + exact; j++){
            if (*(base + i + j) != *(str + j)){
                result = -1;
                break;
            }
        }
        if (result >= 0) break;
    }
    return result;
}
