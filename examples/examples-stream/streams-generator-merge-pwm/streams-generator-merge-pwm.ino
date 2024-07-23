/**
 * @file streams-generator-merge-pwm.ino
 * @brief merge 2 mono signals into 1 stereo signal
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "AudioTools.h"

AudioInfo info_in(8000, 1, 16);
AudioInfo info_out(8000, 2, 16);
SineWaveGenerator<int16_t> sineWave1(32000); // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound1(sineWave1);  // Stream generated from sine wave
SineWaveGenerator<int16_t> sineWave2(32000); // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound2(sineWave2);  // Stream generated from sine wave
InputMerge<int16_t> imerge;
PWMAudioOutput pwm;   
StreamCopy copier(pwm, imerge);    // copy in to out


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);  

  // setup sine 2 mono sine waves
  sineWave1.begin(info_in, N_B4);
  sineWave2.begin(info_in, N_B5);

  // merge input to stereo
  imerge.add(sound1);
  imerge.add(sound2);
  imerge.begin(info_out);

  // setup PWM output
  auto config = pwm.defaultConfig();
  config.copyFrom(info_out);
  //config.resolution = 8;  // must be between 8 and 11 -> drives pwm frequency (8 is default)
  // alternative 1
  //config.start_pin = 3;
  // alternative 2
  //int pins[] = {3};
  // alternative 3
  Pins pins = {3, 4};
  config.setPins(pins); 
  pwm.begin(config);
}

void loop(){
  copier.copy();
}