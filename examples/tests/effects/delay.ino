#include "AudioTools.h"

int channels = 1;
int rate = 44100;
int hz = 200;
SineWaveGenerator<int16_t> sine;
AudioEffects<SineWaveGenerator<int16_t>> effects(sine);
GeneratedSoundStream<int16_t> in(effects); 
CsvStream<int16_t> out(Serial);
StreamCopy copier(out, in); 
//Delay dly;
Delay dly(998, 0.5, 1.0,rate, true);


void setup() {
  Serial.begin(115200);
  // setup effects
  effects.addEffect(dly);

  // Setup output
  auto cfg = out.defaultConfig();
  cfg.channels = channels;
  cfg.sample_rate = rate;
  out.begin(cfg);

  // Setup sound generation based on AudioKit settins
  sine.begin(cfg, hz);
  in.begin(cfg);
}
// copy the data
void loop() {
  copier.copy();
}