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
//#include "knghtsng.h"
#include "alice.h"

//Data Flow: MemoryStream -> EncodedAudioStream  -> PWMAudioOutput
//Use 8000 for alice_wav and 11025 for knghtsng_wav
AudioInfo info(8000, 1, 16);
//MemoryStream wav(knghtsng_wav, knghtsng_wav_len);
MemoryStream wav(alice_wav, alice_wav_len);
PWMAudioOutput pwm;          // PWM output 
EncodedAudioStream out(&pwm, new WAVDecoder()); // Decoder stream
StreamCopy copier(out, wav);    // copy in to out

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  wav.begin();
  out.begin();

  auto config = pwm.defaultConfig();
  config.copyFrom(info);
  pwm.begin(config);
}

void loop(){
  if (wav) {
    copier.copy();
  } else {
    // after we are done we just print some info form the wav file
    auto info = out.audioInfo();
    LOGI("The audio rate from the wav file is %d", info.sample_rate);
    LOGI("The channels from the wav file is %d", info.channels);

    // restart from the beginning
    Serial.println("Restarting...");
    delay(5000);
    out.begin();   // indicate that we process the WAV header
    wav.begin();       // reset actual position to 0
    pwm.begin();       // reset counters 
  }
}