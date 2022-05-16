// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "AudioLibs/PortAudioStream.h"
#include "AudioCodecs/CodecOpusOgg.h"


URLStream url("ssid","pwd");
PortAudioStream portaudio_stream;   // Output of sound on desktop 
EncodedAudioStream dec(&portaudio_stream, new OpusOggDecoder()); // MP3 data source
StreamCopy copier(dec, url); // copy in to out

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  url.begin("http://st1.urbanrevolution.es:8000/zonaelectronica","audio/opus");
  dec.begin();
  portaudio_stream.begin();
}

void loop(){
copier.copy();
}