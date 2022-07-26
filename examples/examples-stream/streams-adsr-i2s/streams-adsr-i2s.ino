/**
 * @file streams-adsr-i2s.ino
 * @author Phil Schatzmann
 * @brief Simple example for AudioEffects with ADSR
 * @copyright GPLv3
 * 
 */
#include "AudioTools.h"
//#include "AudioLibs/AudioKit.h"

I2SStream i2s; //AudioKitStream
SineWaveGenerator<int16_t> sine;
AudioEffects<SineWaveGenerator<int16_t>> effects(sine);
ADSRGain adsr(0.0001,0.0001, 0.9 , 0.0002);
GeneratedSoundStream<int16_t> in(effects); 
StreamCopy copier(i2s, in); 
uint64_t time_on;
uint64_t time_off;


void actionKeyOn(int note){
  Serial.println("KeyOn");
  sine.setFrequency(note);
  adsr.keyOn();
}

void actionKeyOff(){
  Serial.println("KeyOff");
  adsr.keyOff();
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Warning);

  // setup effects
  effects.addEffect(adsr);

  // Setup output
  auto cfg = i2s.defaultConfig(TX_MODE);
  //i2s.setVolume(1.0);
  i2s.begin(cfg);

  // Setup sound generation based on AudioKit settins
  sine.begin(cfg, 0);
  in.begin(cfg);

}

// copy the data
void loop() {
  copier.copy();

  uint64_t time = millis();
  if (time>time_off){
    actionKeyOff();
    time_on = time+1000;
    time_off = time+2000;
  }
  if (time>time_on){
    actionKeyOn(N_C3);
    time_on = time+2000;
    time_off = time+1000;
  }
}
