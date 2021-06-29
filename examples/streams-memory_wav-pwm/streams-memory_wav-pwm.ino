/**
 * @file stream-memory_wav-pwm.ino
 * @author Phil Schatzmann
 * @brief decode WAV stream and output to PWM pins
 * @version 0.1
 * @date 2021-01-24
 * 
 * @copyright Copyright (c) 2021
 */

#include "AudioTools.h"
#include "CodecWAV.h"
#include "knghtsng.h"

using namespace audio_tools;  

// MemoryStream -> AudioOutputStream -> WAVDecoder -> CsvStream
MemoryStream wav(knghtsng_wav, knghtsng_wav_len);
PWMAudioStream pwm;          // PWM output 
WAVDecoder decoder(pwm);        // decode wav to pcm and send it to printer
AudioOutputStream out(decoder); // output to decoder
StreamCopy copier(out, wav);    // copy in to out

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Debug);  

  // setup pwm output
  PWMConfig config = pwm.defaultConfig();
  config.channels = 1;
  config.sample_rate = 1000;
  pwm.begin(config);
}

void loop(){
  if (wav) {
    copier.copy();
  } else {
    // after we are done we just print some info form the wav file
    auto info = decoder.audioInfo();
    LOGI("The audio rate from the wav file is %d", info.sample_rate);
    LOGI("The channels from the wav file is %d", info.channels);
    stop();
  }
}