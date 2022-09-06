/**
   @file streams-sam-audiokit.ino

   @author Phil Schatzmann
   @copyright GPLv3

*/

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "sam_arduino.h"

AudioKitStream kit;
SAM sam(kit);

const char* text = "Hallo my name is SAM";
void setup(){
  Serial.begin(115200);
  
  // setup audiokit i2s 
  auto cfg = kit.defaultConfig();
  cfg.bits_per_sample = sam.bitsPerSample();
  cfg.channels = sam.channels();
  cfg.sample_rate = sam.sampleRate();
  cfg.sd_active = false;
  kit.begin(cfg);

  sam.say(text);
}

void loop() {
  // feed watchdog
  delay(100);
}