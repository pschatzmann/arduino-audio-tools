#include "AudioTools.h"

AudioInfo info(44100, 1, 16);
SineWaveGenerator<int16_t> sineWave(32000);               // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in(sineWave);               // Stream generated from sine wave
CsvOutput<int16_t> out;                                   // Or use StdOuput 
StreamCopy copier(out, in); // copy in to out

// Arduino Setup
void setup(void) {
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // open output
  auto config = out.defaultConfig();
  config.copyFrom(info);
  out.begin(config);

  // Setup sine wave
  sineWave.begin(info, N_B4);

}

// Arduino loop  
void loop() {
  copier.copy();
}