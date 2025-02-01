#include "AudioTools.h"


SineWaveGeneratorT<int16_t> sineWave(32000);                // subclass of SoundGeneratorT with max amplitude of 32000
GeneratedSoundStreamT<int16_t> sound(sineWave);             // Stream generated from sine wave
CsvOutput out(Serial); 
ResampleStream resample(out);
StreamCopy copier(resample, sound);                        // copies sound to out
AudioInfo info(44100,2,16);

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

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