/**
 * @file streams-sam-i2s.ino
 * You need to install https://github.com/pschatzmann/SAM
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
#include "AudioTools.h"
#include "sam_arduino.h"
//#include "AudioLibs/AudioBoardStream.h"


I2SStream out; // Replace with desired class e.g. AudioBoardStream, AnalogAudioStream etc.
SAM sam(out, false);

// Callback which provides the audio data 
void outputData(Print *out){
}

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

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