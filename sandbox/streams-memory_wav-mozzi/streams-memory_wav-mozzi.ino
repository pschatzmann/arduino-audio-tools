/**
 * @file stream-memory_wav-mozzi.ino
 * @author Phil Schatzmann
 * @brief decode WAV stream and output to mozzi pins with Mozzi
 * @version 0.1
 * @date 2021-01-24
 * 
 * @copyright Copyright (c) 2021
 */


#include "AudioTools.h"
#include "AudioMozzi.h"
//#include "knghtsng.h"
#include "alice.h"

using namespace audio_tools;  

//Data Flow: MemoryStream -> AudioOutputStream -> WAVDecoder -> MozziStream

//MemoryStream wav(knghtsng_wav, knghtsng_wav_len);
MemoryStream wav(alice_wav, alice_wav_len);
MozziStream mozzi;          // mozzi output 
WAVDecoder decoder(mozzi);        // decode wav to pcm and send it to printer
AudioOutputStream out(decoder); // output to decoder
StreamCopy copier(out, wav);    // copy in to out

void setup(){
  Serial.begin(115200);
  while(!Serial);
  AudioLogger::instance().begin(Serial, AudioLogger::Debug);  

  // setup mozzi output
  config.channels = 1;
  //config.sample_rate = 11025;  // for knghtsng_wav
  config.sample_rate = 8000;  // for alice_wav
  mozzi.begin(config);
}

void loop(){
  if (wav) {
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
    mozzi.begin();       // reset counters 
  }
}