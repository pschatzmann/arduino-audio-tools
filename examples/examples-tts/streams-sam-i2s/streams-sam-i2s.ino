/**
 * @file streams-sam-i2s.ino
 * You need to install https://github.com/pschatzmann/SAM
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
#include "AudioTools.h"
#include "sam_arduino.h"
//#include "AudioTools/AudioLibs/AudioBoardStream.h"


I2SStream out; // Replace with desired class e.g. AudioBoardStream, AnalogAudioStream etc.
SAM sam(out, false);


void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // start data sink
  auto cfg = out.defaultConfig();
  cfg.sample_rate = SAM::sampleRate();
  cfg.channels = 1;
  cfg.bits_per_sample = 16;
  out.begin(cfg);

}


// Arduino loop  
void loop() {
  Serial.println("providing data...");
  sam.say("Hallo, my name is Alice");
  delay(5000);
}