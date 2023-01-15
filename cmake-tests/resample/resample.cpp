// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"

uint16_t sample_rate=44100;
uint8_t channels = 1;                                      // The stream will have 2 channels 
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
ResampleStream<int16_t> out(sound);                        // We double the output sample rate

CsvStream<int16_t> csv(Serial, channels);                  // Output to Serial
StreamCopy copier(csv, out, 1012);                       // copies sound to out

// Arduino Setup
void setup(void) {  
  // Open Serial 
  //Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  auto config = out.defaultConfig();
  config.sample_rate = sample_rate; 
  config.channels = channels;
  config.step_size = 1.9;
  out.begin(config);

  // Define CSV Output
  csv.begin(config);
  sound.begin(config);
  sineWave.begin(config, N_B4);
  Serial.println("started (mixer)...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
  Serial.println("----");
}
