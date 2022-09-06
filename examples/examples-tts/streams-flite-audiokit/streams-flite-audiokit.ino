/**
   @file streams-flite-audiokit.ino

   @author Phil Schatzmann
   @copyright GPLv3

*/

#include "flite_arduino.h"
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

AudioKitStream kit;
Flite flite(kit);

const char* alice = "Hallo my name is FLITE";

void setup(){
  Serial.begin(115200);
  auto cfg = kit.defaultConfig();
  cfg.bits_per_sample = 16;
  cfg.channels = 1;
  cfg.sample_rate = 8000;
  cfg.sd_active = false;
  kit.begin(cfg);
  
  flite.say(alice);
}

void loop() {
}