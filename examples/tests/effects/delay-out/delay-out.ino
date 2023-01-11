#include "AudioTools.h"

int channels = 1;
int rate = 44100;
int hz = 200;
SineWaveGenerator<int16_t> sine;
GeneratedSoundStream<int16_t> gen(sine); 
CsvStream<int16_t> csv(Serial);
AudioEffectStream effects(csv); // apply effects to output: writing to effects
StreamCopy copier(effects, gen); 
Delay dly(998, 0.5, 1.0,rate, true);


void setup() {
  Serial.begin(115200);
  // setup effects
  effects.addEffect(dly);

  // Setup audio objects
  auto cfg = csv.defaultConfig();
  cfg.channels = channels;
  cfg.sample_rate = rate;
  csv.begin(cfg);
  sine.begin(cfg, hz);
  gen.begin(cfg);
  effects.begin(cfg);
}
// copy the data
void loop() {
  copier.copy();
}