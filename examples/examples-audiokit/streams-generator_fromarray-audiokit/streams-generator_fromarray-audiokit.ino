/**
 * @file streams-generator_fromarray-audiokit.ino
 * @brief Tesing GeneratorFromArray with output on audiokit
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

uint16_t sample_rate=44100
;
uint8_t channels = 2;                                      // The stream will have 2 channels 
int16_t sine_array[] = {0, 2285, 4560, 6812, 9031, 11206, 13327, 15383, 17363, 19259, 21062, 22761, 24350, 25820, 27165, 28377, 29450, 30381, 31163, 31793, 32269, 32587, 32747, 32747, 32587, 32269, 31793, 31163, 30381, 29450, 28377, 27165, 25820, 24350, 22761, 21062, 19259, 17363, 15383, 13327, 11206, 9031, 6812, 4560, 2285, 0, -2285, -4560, -6812, -9031, -11206, -13327, -15383, -17363, -19259, -21062, -22761, -24350, -25820, -27165, -28377, -29450, -30381, -31163, -31793, -32269, -32587, -32747, -32747, -32587, -32269, -31793, -31163, -30381, -29450, -28377, -27165, -25820, -24350, -22761, -21062, -19259, -17363, -15383, -13327, -11206, -9031, -6812, -4560, -2285 };
GeneratorFromArray<int16_t> sineWave(sine_array,0,false);
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
AudioKitStream out; 

StreamCopy copier(out, sound);                             // copies sound into i2s

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.sample_rate = sample_rate; 
  config.channels = channels;
  config.bits_per_sample = 16;
  out.begin(config);

  // Setup sine wave
  auto cfg = sineWave.defaultConfig();
  cfg.channels = channels;
  cfg.sample_rate = sample_rate;
  sineWave.begin(cfg);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}