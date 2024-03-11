/**
 * @file streams-tts-i2s.ino
 * You need to install https://github.com/pschatzmann/tts
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#include "AudioTools.h"
#include "TTS.h"
//#include "AudioLibs/AudioBoardStream.h"

I2SStream out; // Replace with desired class e.g. AudioBoardStream, AnalogAudioStream etc.
TTS tts = TTS(out);


void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start data sink
  TTSInfo info = TTS::getInfo();
  auto cfg = out.defaultConfig();
  cfg.sample_rate = info.sample_rate;
  cfg.channels = info.channels;
  cfg.bits_per_sample = info.bits_per_sample;
  out.begin(cfg);
}


// Arduino loop  
void loop() {
  Serial.println("providing data...");
  tts.sayText("Hallo, my name is Alice");
  delay(5000);
}
