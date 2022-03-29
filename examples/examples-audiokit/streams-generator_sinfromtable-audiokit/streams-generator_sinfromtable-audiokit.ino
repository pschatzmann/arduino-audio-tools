/**
 * @file streams-generator_sinfromtable-audiokit.ino
 * @brief Tesing SineFromTable with output on audiokit
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

uint16_t sample_rate=32000;
uint8_t channels = 2;                                      // The stream will have 2 channels 
SineFromTable<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
AudioKitStream out; 
//CsvStream<int16_t> out(Serial);
int sound_len=1024;
StreamCopy copier(out, sound, sound_len);                             // copies sound into i2s
int freq = 122;

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(); //TX_MODE
  config.sample_rate = sample_rate; 
  config.channels = channels;
  config.bits_per_sample = 16;
  out.begin(config);

  // Setup sine wave
  auto cfg = sineWave.defaultConfig();
  cfg.channels = channels;
  cfg.sample_rate = sample_rate;
  sineWave.begin(cfg, freq);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  // the length is defined by sound_len
  copier.copy();
  // increase frequency
  freq += 10;
  if (freq>=10000){
    freq = 20;
  }
  sineWave.setFrequency(freq);
}