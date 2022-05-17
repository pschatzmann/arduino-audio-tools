#include "AudioTools.h"
#include "AudioLibs/PortAudioStream.h"

uint16_t sample_rate=44100;
uint8_t channels = 1;                                     // The stream will have 2 channels 
SineWaveGenerator<int16_t> sineWave(32000);               // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in(sineWave);               // Stream generated from sine wave
PortAudioStream out;                                        // On desktop we use 
StreamCopy copier(out, in); // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // open output
  auto config = out.defaultConfig();
  config.sample_rate = sample_rate;
  config.channels = channels;
  config.bits_per_sample = sizeof(int16_t)*8;
  out.begin(config);

  // Setup sine wave
  sineWave.begin(channels, sample_rate, N_B4);

}

// Arduino loop  
void loop() {
  if (out)
    copier.copy();
}