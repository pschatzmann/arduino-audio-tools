/**
 * @file stream-memory_mp3-pwm.ino
 * @author Phil Schatzmann
 * @brief decode mp3 stream and output to PWM pins
 * @version 0.1
 * @date 2021-01-24
 * 
 * @copyright Copyright (c) 2021
 */

#include "AudioTools.h"
#include "BabyElephantWalk60_mp3.h"

using namespace audio_tools;  

//Data Flow: MemoryStream -> AudioOutputStream -> WAVDecoder -> PWMAudioStream

//MemoryStream wav(knghtsng_wav, knghtsng_wav_len);
MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
PWMAudioStream pwm;             // PWM output 
MP3DecoderMini decoder(pwm);    // decode wav to pcm and send it to printer
AudioOutputStream out(decoder); // output to decoder
StreamCopy copier(out, mp3);    // copy in to out
PWMConfig config = pwm.defaultConfig();

void setup(){
  Serial.begin(115200);
  while(!Serial);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // setup pwm output
  config.channels = 1;
  config.sample_rate = 22050;  // for BabyElephantWalk60_mp3
  pwm.begin(config);
}

void loop(){
  if (mp3) {
    copier.copy();
  } else {
    // after we are done we just print some info form the wav file
    auto info = decoder.audioInfo();
    LOGI("The audio rate from the wav file is %d", info.sample_rate);
    LOGI("The channels from the wav file is %d", info.channels);

    // restart from the beginning
    Serial.println("Restarting...");
    delay(5000);
    decoder.begin();   // indicate that we process the WAV header
    wav.begin();       // reset actual position to 0
    mp3.begin();       // reset counters 
  }
}