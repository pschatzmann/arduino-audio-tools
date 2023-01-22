#include "AudioTools.h"


uint16_t sample_rate=44100;
uint8_t channels = 2;                                      // The stream will have 2 channels 
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
CsvStream<int16_t> out(Serial); 
ResampleStream<int16_t> resample(out);
StreamCopy copier(resample, sound);                        // copies sound to out
AudioBaseInfo info;

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // setup audio info
  info.channels = channels;
  info.sample_rate = sample_rate;
  info.bits_per_sample = 16;

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
  sineWave.begin(channels, sample_rate, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}