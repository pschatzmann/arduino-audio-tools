#include "AudioTools.h"


SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
CsvOutput<int16_t> out(Serial); 
ResampleStream resample(out);
StreamCopy copier(resample, sound);                        // copies sound to out
AudioInfo info(44100,2,16);

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // define resample
  auto rcfg = resample.defaultConfig();
  rcfg.copyFrom(info);
  rcfg.step_size = 0.5;
  resample.begin(rcfg); 

  // Define CSV Output
  auto config = out.defaultConfig();
  config.copyFrom(info);
  out.begin(config);

  // Setup sine wave
  sineWave.begin(info, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}