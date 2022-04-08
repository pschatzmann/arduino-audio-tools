/**
 * @file streams-generator-audiokit.ino
 * @brief Tesing I2S output on audiokit
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

uint16_t sample_rate=32000;
uint8_t channels = 2;                                      // The stream will have 2 channels 
SineWaveGenerator<int16_t> sineWave1(32000);                // subclass of SoundGenerator with max amplitude of 32000
SineWaveGenerator<int16_t> sineWave2(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound1(sineWave1);             // Stream generated from sine wave
GeneratedSoundStream<int16_t> sound2(sineWave2);             // Stream generated from sine wave
Mixer<int16_t> mixer;
AudioKitStream out; 
StreamCopy copier(out, mixer);                             // copies sound into i2s

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  mixer.add(sound1);
  mixer.add(sound2);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.sample_rate = sample_rate; 
  config.channels = channels;
  config.bits_per_sample = 16;
  out.begin(config);

  // Setup sine wave
  sineWave1.begin(channels, sample_rate, N_B4);
  sineWave2.begin(channels, sample_rate, N_E4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
