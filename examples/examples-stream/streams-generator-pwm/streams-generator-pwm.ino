/**
 * @file streams-generator-pwm.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-generator-pwm/README.md 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "AudioTools.h"

//Pins pins = {22, 23};
int channels = 1;
uint16_t sample_rate=8000;
SineWaveGenerator<int16_t> sineWave(32000); // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);  // Stream generated from sine wave
PWMAudioStream pwm;                  
StreamCopy copier(pwm, sound);    // copy in to out


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);  

  // setup sine wave
  sineWave.begin(channels, sample_rate, N_B4);

  // setup PWM output
  auto config = pwm.defaultConfig();
  config.sample_rate = sample_rate;
  //config.resolution = 8;  // must be between 8 and 11 -> drives pwm frequency (8 is default)
  // alternative 1
  //config.channels = 2;  // not necesarry because 2 is default
  //config.start_pin = 3;
  // alternative 2: defines pins and channels
  //config.setPins(pins); 
  pwm.begin(config);
}

void loop(){
  copier.copy();
}