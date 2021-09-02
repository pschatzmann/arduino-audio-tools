// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "BabyElephantWalk60_mp3.h"

using namespace audio_tools;  

MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
MetaDataID3 out;
StreamCopy copier(out, mp3); // copy in to out
bool title_printed = false;


void printTitle(const char* title, int len){
  Serial.print("==> Title: ");
  Serial.println(title);
  title_printed = true;
}

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  out.setTitleCallback(printTitle);
  out.begin();
  mp3.begin();
}

void loop(){
  if (mp3) {
    copier.copy();
  } else {
    assert(title_printed);
    exit(0);
  }
}

int main(){
  setup();
  while(true) loop();
}