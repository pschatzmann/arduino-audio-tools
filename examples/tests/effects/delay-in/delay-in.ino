#include "AudioTools.h"

int hz = 200;
AudioInfo info(44100, 1, 16);
SineWaveGeneratorT<int16_t> sine;
GeneratedSoundStreamT<int16_t> gen(sine); 
AudioEffectStream effects(gen); // apply effects to input: reading from gen
CsvOutput out(Serial);
StreamCopy copier(out, effects); 
Delay dly(998, 0.5, 1.0, info.sample_rate);


void setup() {
  Serial.begin(115200);
  
  // setup effects
  effects.addEffect(dly);

  // Setup audio objects
  out.begin(info);
  sine.begin(info, hz);
  gen.begin(info);
  effects.begin(info);
}
// copy the data
void loop() {
  copier.copy();
}