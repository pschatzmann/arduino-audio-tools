#include "AudioTools.h"

URLStream url("SSID","PASSWORD");  // or replace with ICYStream to get metadata
MeasuringStream out(50, &Serial); // final output of decoded stream
StreamCopy copier(out, url); // copy url to decoder

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);  
  while(!Serial);

  url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128","audio/mp3");
}

void loop(){
  copier.copy();
}
