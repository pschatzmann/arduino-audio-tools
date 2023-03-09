// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "AudioLibs/PortAudioStream.h"

using namespace audio_tools;  

PortAudioStream out;
SineWaveGenerator<int16_t> sine;
AudioEffects<SineWaveGenerator<int16_t>> effects(sine);
ADSRGain adsr(0.0001,0.0001, 0.9 , 0.0002);
GeneratedSoundStream<int16_t> in(effects); 
StreamCopy copier(out, in); 
uint64_t timeout=0;
float freq = N_C4;


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Warning);

  // setup effects
  effects.addEffect(adsr);

  // Setup output
  auto cfg = out.defaultConfig();
  cfg.channels = 1;
  cfg.bits_per_sample = 16;
  cfg.sample_rate = 44100;
  out.begin(cfg);

  // Setup sound generation based on AudioKit settins
  effects.begin(cfg);
  sine.begin(cfg, 0);
  in.begin(cfg);
  sine.setFrequency(freq);
}


void loop() {
  // simulate key press
  if (millis()>timeout){
    sine.setFrequency(freq);
    adsr.keyOn();
    timeout = millis()+3000;
  }
  // simulate key release
  if (millis()>timeout+1000){
    sine.setFrequency(freq);
    adsr.keyOff();
  }
  // output audio
  copier.copy();
}

