/**
 * @file streams-flite-i2s.ino
 * You need to install https://github.com/pschatzmann/arduino-flite
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#include "flite_arduino.h"
#include "AudioTools.h"
//#include "AudioTools/AudioLibs/AudioBoardStream.h"

I2SStream out; // Replace with desired class e.g. AudioBoardStream, AnalogAudioStream etc.
Flite flite(out);


void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // start data sink
  auto cfg = out.defaultConfig();
  cfg.sample_rate = 8000;
  cfg.channels = 1;
  cfg.bits_per_sample = 16;
  out.begin(cfg);
}

// Arduino loop  
void loop() {  
  Serial.println("providing data...");
  flite.say("Hallo, my name is Alice");
  delay(5000);
}
