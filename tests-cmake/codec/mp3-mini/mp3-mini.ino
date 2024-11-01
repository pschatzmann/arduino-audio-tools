// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#define MINIMP3_IMPLEMENTATION

#include "Arduino.h"
#include "AudioTools.h"
#include "BabyElephantWalk60_mp3.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Mini.h"

MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
PortAudioStream out;   // Output of sound on desktop 
EncodedAudioStream dec(&out, new MP3DecoderMini()); // MP3 data source
StreamCopy copier(dec, mp3); // copy in to out

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  out.begin();
  mp3.begin();
  dec.begin();
}

void loop(){
  if (mp3) {
    copier.copy();
  } else {
    auto info = dec.decoder().audioInfo();
    LOGI("The audio rate from the mp3 file is %d", info.sample_rate);
    LOGI("The channels from the mp3 file is %d", info.channels);
    exit(0);
  }
}