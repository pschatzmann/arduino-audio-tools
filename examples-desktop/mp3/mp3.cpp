// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "BabyElephantWalk60_mp3.h"

using namespace audio_tools;  


MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
EncodedAudioStream in(mp3, new MP3DecoderMini()); // MP3 data source
PortAudioStream portaudio_stream;   // Output of sound on desktop 
StreamCopy copier(portaudio_stream, in); // copy in to out

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  in.setNotifyAudioBaseInfoChange(portaudio_stream);
  in.begin();

  portaudio_stream.begin();
}

void loop(){
  if (mp3) {
    copier.copy();
  } else {
    auto info = in.decoder().audioInfo();
    LOGI("The audio rate from the mp3 file is %d", info.sample_rate);
    LOGI("The channels from the mp3 file is %d", info.channels);
    exit(0);
  }
}