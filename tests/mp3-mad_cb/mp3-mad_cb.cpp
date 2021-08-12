// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "AudioCodecs/CodecMP3MAD.h"
#include "BabyElephantWalk60_mp3.h"

using namespace audio_tools;  

MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
PortAudioStream portaudio_stream;   // Output of sound on desktop 
MP3DecoderMAD dec; // MP3 data source

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  portaudio_stream.begin();

  dec.setInputStream(mp3);
  dec.setOutputStream(portaudio_stream);
  dec.setNotifyAudioChange(portaudio_stream);
  dec.begin();

}

void loop(){
}

int main(){
  setup();
  while(true) loop();
}