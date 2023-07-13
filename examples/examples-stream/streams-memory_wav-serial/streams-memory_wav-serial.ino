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
#include "knghtsng.h"

// MemoryStream -> EncodedAudioStream -> CsvOutput
MemoryStream wav(knghtsng_wav, knghtsng_wav_len);
CsvOutput<int16_t> out(Serial, 1);  // ASCII stream 
EncodedAudioStream enc(&out, new WAVDecoder());
StreamCopy copier(enc, wav);    // copy in to out

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // update number of channels from wav file
  enc.begin();
  out.begin();
  wav.begin();
}

void loop(){
  if (wav) {
    copier.copy();
  } else {
    auto info = enc.decoder().audioInfo();
    LOGI("The audio rate from the wav file is %d", info.sample_rate);
    LOGI("The channels from the wav file is %d", info.channels);
    stop();
  }
}