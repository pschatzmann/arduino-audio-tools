/**
 * @file stream-memory_wav-serial.ino
 * @author Phil Schatzmann
 * @brief decode WAV strea
 * @version 0.1
 * @date 2021-01-24
 * 
 * @copyright Copyright (c) 2021
 
 */
#include "AudioTools.h"
#include "AudioWAV.h"
#include "knghtsng.h"

using namespace audio_tools;  

// MemoryStream -> AudioOutputStream -> WAVDecoder -> CsvStream
MemoryStream wav(knghtsng_wav, knghtsng_wav_len);
CsvStream<int16_t> printer(Serial, 1);  // ASCII stream 
WAVDecoder decoder(printer);    // decode wav to pcm and send it to printer
AudioOutputStream out(decoder); // output to decoder
StreamCopy copier(out, wav);    // copy in to out

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Debug);  
  // this is not really necessary
  decoder.begin();
}

void loop(){
  if (wav) {
    copier.copy();
  } else {
    auto info = decoder.audioInfo();
    LOGI("The audio rate from the wav file is %d", info.sample_rate);
    LOGI("The channels from the wav file is %d", info.channels);
    stop();
  }
}