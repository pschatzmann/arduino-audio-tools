/**
 * @file streams-generator-i2s.ino
 * @author Phil Schatzmann
 * @brief Test
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "spdif.h"



typedef int16_t sound_t;                                   // sound will be represented as int16_t (with 2 bytes)
uint16_t sample_rate=44100;
uint8_t channels = 2;                                      // The stream will have 2 channels 
SineWaveGenerator<sound_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<sound_t> sound(sineWave);             // Stream generated from sine wave
uint8_t buffer[1024];

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start I2S
  Serial.println("starting SPDIF...");
  auto config = out.defaultConfig();
  config.sample_rate = sample_rate; 
  config.channels = channels;
  config.pin_data = 23;
  out.begin(config);

  // Setup sine wave
  sineWave.begin(channels, sample_rate, N_B4);
  Serial.println("started...");

  spdif_init(sample_rate);
}

// Arduino loop - copy sound to out 
void loop() {
  int read = sound.readBytes(buffer, 1024);
  spdif_write(buffer, read);

}
