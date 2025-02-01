#include "AudioTools.h"

AudioInfo info(44100, 1, 16);
SineWaveGeneratorT<int16_t> sineWave(32000);               // subclass of SoundGeneratorT with max amplitude of 32000
GeneratedSoundStreamT<int16_t> in(sineWave);               // Stream generated from sine wave
CsvOutput out;                                   // Or use StdOuput 
StreamCopy copier(out, in); // copy in to out

// Arduino Setup
void setup(void) {
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

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