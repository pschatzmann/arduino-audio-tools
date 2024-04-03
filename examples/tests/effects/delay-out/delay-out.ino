#include "AudioTools.h"

AudioInfo info(44100, 1, 16);
int hz = 200;
SineWaveGenerator<int16_t> sine;
GeneratedSoundStream<int16_t> gen(sine); 
CsvOutput<int16_t> csv(Serial);
AudioEffectStream effects(csv); // apply effects to output: writing to effects
StreamCopy copier(effects, gen); 
Delay dly(998, 0.5, 1.0,info.sample_rate);


void setup() {
  Serial.begin(115200);
  // setup effects
  effects.addEffect(dly);

  // Setup audio objects
  csv.begin(info);
  sine.begin(info, hz);
  gen.begin(info);
  effects.begin(info);
}
// copy the data
void loop() {
  copier.copy();
}