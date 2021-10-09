/**
 * @file test-memory_wav-decode.ino
 * @author Phil Schatzmann
 * @brief decode WAV strea
 * @version 0.1
 * @date 2021-01-24
 * 
 * @copyright Copyright (c) 2021
 
 */
#include "AudioTools.h"
#include "knghtsng.h"

using namespace audio_tools;  

MemoryStream wav(knghtsng_wav, knghtsng_wav_len);
CsvStream<int16_t> printer(Serial, 1); // ASCII stream 
WAVDecoder decoder(printer);
const int buffer_len = 1024;
char buffer[buffer_len];

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Debug);

  int len = wav.readBytes(buffer, buffer_len);
  decoder.write(buffer, len);
  
  auto info = decoder.audioInfo();
  LOGI("The audio rate from the wav file is %d", info.sample_rate);
  LOGI("The channels from the wav file is %d", info.channels);
  
}

void loop(){
  int len = wav.readBytes(buffer, buffer_len);
  decoder.write(buffer, len);  
}