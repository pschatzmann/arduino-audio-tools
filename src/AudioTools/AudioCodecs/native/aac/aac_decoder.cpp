/*
 *  aac_decoder.cpp
 *  faad2 - ESP32 adaptation
 *  Created on: 12.09.2023
 *  Updated on: 04.10.2024
*/

#include "Arduino.h"
#include "aac_decoder.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "libfaad/neaacdec.h"


// Declaration of the required global variables

NeAACDecHandle hAac;
NeAACDecFrameInfo frameInfo;
NeAACDecConfigurationPtr conf;
const uint8_t  SYNCWORDH = 0xff; /* 12-bit syncword */
const uint8_t  SYNCWORDL = 0xf0;
bool f_decoderIsInit = false;
bool f_firstCall = false;
bool f_setRaWBlockParams = false;
uint32_t aacSamplerate = 0;
uint8_t aacChannels = 0;
uint8_t aacProfile = 0;
static uint16_t validSamples = 0;
clock_t before;
float compressionRatio = 1;
mp4AudioSpecificConfig* mp4ASC;

//----------------------------------------------------------------------------------------------------------------------
bool AACDecoder_IsInit(){
    return f_decoderIsInit;
}
//----------------------------------------------------------------------------------------------------------------------
bool AACDecoder_AllocateBuffers(){
    before = clock();
    hAac = NeAACDecOpen();
    conf = NeAACDecGetCurrentConfiguration(hAac);

    if(hAac) f_decoderIsInit = true;
    f_firstCall = false;
    f_setRaWBlockParams = false;
    return f_decoderIsInit;
}
//----------------------------------------------------------------------------------------------------------------------
void AACDecoder_FreeBuffers(){
    NeAACDecClose(hAac);
    hAac = NULL;
    f_decoderIsInit = false;
    f_firstCall = false;
    clock_t difference = clock() - before;
    int msec = difference  / CLOCKS_PER_SEC; (void)msec;
//    printf("ms %li\n", difference);
}
//----------------------------------------------------------------------------------------------------------------------
uint8_t AACGetFormat(){
    return frameInfo.header_type;  // RAW        0 /* No header */
                                   // ADIF       1 /* single ADIF header at the beginning of the file */
                                   // ADTS       2 /* ADTS header at the beginning of each frame */
}
//----------------------------------------------------------------------------------------------------------------------
uint8_t AACGetSBR(){
    return frameInfo.sbr;          // NO_SBR           0 /* no SBR used in this file */
                                   // SBR_UPSAMPLED    1 /* upsampled SBR used */
                                   // SBR_DOWNSAMPLED  2 /* downsampled SBR used */
                                   // NO_SBR_UPSAMPLED 3 /* no SBR used, but file is upsampled by a factor 2 anyway */
}
//----------------------------------------------------------------------------------------------------------------------
uint8_t AACGetParametricStereo(){  // not used (0) or used (1)
//    log_w("frameInfo.ps %i", frameInfo.isPS);
    return frameInfo.isPS;
}
//----------------------------------------------------------------------------------------------------------------------
int AACFindSyncWord(uint8_t *buf, int nBytes){
    int i;

    /* find byte-aligned syncword (12 bits = 0xFFF) */
    for (i = 0; i < nBytes - 1; i++) {
        if ( (buf[i+0] & SYNCWORDH) == SYNCWORDH && (buf[i+1] & SYNCWORDL) == SYNCWORDL ){
            return i;
        }
    }

    return -1;
}
//----------------------------------------------------------------------------------------------------------------------
int AACSetRawBlockParams(int nChans, int sampRateCore, int profile){
    f_setRaWBlockParams = true;
    aacChannels = nChans;  // 1: Mono, 2: Stereo
    aacSamplerate = (uint32_t)sampRateCore; // 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
    aacProfile = profile; //1: AAC Main, 2: AAC LC (Low Complexity), 3: AAC SSR (Scalable Sample Rate), 4: AAC LTP (Long Term Prediction)
    return 0;
}
//----------------------------------------------------------------------------------------------------------------------
int16_t AACGetOutputSamps(){
    return validSamples;
}
//----------------------------------------------------------------------------------------------------------------------
int AACGetBitrate(){
    uint32_t br = AACGetBitsPerSample() * AACGetChannels() *  AACGetSampRate();
    return (br / compressionRatio);;
}
//----------------------------------------------------------------------------------------------------------------------
int AACGetChannels(){
    return aacChannels;
}
//----------------------------------------------------------------------------------------------------------------------
int AACGetSampRate(){
    return aacSamplerate;
}
//----------------------------------------------------------------------------------------------------------------------
int AACGetBitsPerSample(){
    return 16;
}
//----------------------------------------------------------------------------------------------------------------------
void createAudioSpecificConfig(uint8_t* config, uint8_t audioObjectType, uint8_t samplingFrequencyIndex, uint8_t channelConfiguration) {
    config[0] = (audioObjectType << 3) | (samplingFrequencyIndex >> 1);
    config[1] = (samplingFrequencyIndex << 7) | (channelConfiguration << 3);
}
//----------------------------------------------------------------------------------------------------------------------
extern uint8_t get_sr_index(const uint32_t samplerate);

int AACDecode(uint8_t *inbuf, int32_t *bytesLeft, short *outbuf){
    uint8_t* ob = (uint8_t*)outbuf;
    if (f_firstCall == false){
        if(f_setRaWBlockParams){ // set raw AAC values, e.g. for M4A config.
            f_setRaWBlockParams = false;
            conf->defSampleRate = aacSamplerate;
            conf->outputFormat = FAAD_FMT_16BIT;
            conf->useOldADTSFormat = 1;
            conf->defObjectType = 2;
            int8_t ret = NeAACDecSetConfiguration(hAac, conf); (void)ret;

            uint8_t specificInfo[2];
            createAudioSpecificConfig(specificInfo, aacProfile, get_sr_index(aacSamplerate), aacChannels);
            int8_t err = NeAACDecInit2(hAac, specificInfo, 2, &aacSamplerate, &aacChannels);(void)err;
        }
        else{
            NeAACDecSetConfiguration(hAac, conf);
            int8_t err = NeAACDecInit(hAac, inbuf, *bytesLeft, &aacSamplerate, &aacChannels); (void)err;
        }
        f_firstCall = true;
    }

    NeAACDecDecode2(hAac, &frameInfo, inbuf, *bytesLeft, (void**)&ob, 2048 * 2 * sizeof(int16_t));
    *bytesLeft -= frameInfo.bytesconsumed;
    validSamples = frameInfo.samples;
    int8_t err = 0 - frameInfo.error;
    compressionRatio = (float)frameInfo.samples * 2 / frameInfo.bytesconsumed;
    return err;
}
//----------------------------------------------------------------------------------------------------------------------
const char* AACGetErrorMessage(int8_t err){
    return NeAACDecGetErrorMessage(abs(err));
}
//----------------------------------------------------------------------------------------------------------------------
