#include "AudioTools.h"

using namespace audio_tools;  

URLStream url("ssid","password");
NullStream null_out; // final output of decoded stream
StreamCopy copier(null_out, url); // copy url to decoder


void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  
// mp3 radio
  url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128","audio/mp3");
}

void loop(){
  if (!copier.copy()) {
    stop();
  }
}

