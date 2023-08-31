/**
 * @file streams-generator-pwm.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-generator-pwm/README.md 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "AudioTools.h"

AudioInfo info(8000, 1, 16);
SineWaveGenerator<int16_t> sineWave(32000); // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);  // Stream generated from sine wave
PWMAudioOutput pwm;                  
StreamCopy copier(pwm, sound);    // copy in to out


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);  

  // setup sine wave
  sineWave.begin(info, N_B4);

  // setup PWM output
  auto config = pwm.defaultConfig();
  config.copyFrom(info);
  //config.resolution = 8;  // must be between 8 and 11 -> drives pwm frequency (8 is default)
  // alternative 1
  //config.start_pin = 3;
  // alternative 2
  //int pins[] = {3};
  // alternative 3
  //Pins pins = {3};
  //config.setPins(pins); 
  pwm.begin(config);
}

void loop(){
  copier.copy();
}