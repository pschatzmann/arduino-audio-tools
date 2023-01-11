#include "AudioTools.h"

int channels = 1;
int rate = 44100;
int hz = 200;
SineWaveGenerator<int16_t> sine;
GeneratedSoundStream<int16_t> gen(sine); 
AudioEffectStream effects(gen); // apply effects to input: reading from gen
CsvStream<int16_t> out(Serial);
StreamCopy copier(out, effects); 
Delay dly(998, 0.5, 1.0,rate, true);


void setup() {
  Serial.begin(115200);
  // setup effects
  effects.addEffect(dly);

  // Setup audio objects
  auto cfg = out.defaultConfig();
  cfg.channels = channels;
  cfg.sample_rate = rate;
  out.begin(cfg);
  sine.begin(cfg, hz);
  gen.begin(cfg);
  effects.begin(cfg);
}
// copy the data
void loop() {
  copier.copy();
}