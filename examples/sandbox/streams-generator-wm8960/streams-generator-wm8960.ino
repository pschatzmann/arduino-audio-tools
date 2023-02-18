/**
 * @file streams-generator-wm8990.ino
 * @author Phil Schatzmann
 * @brief Test sketch for wm8990
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "AudioLibs/WM8960Stream.h"

uint16_t sample_rate=44100;
uint8_t channels = 2;                                      // The stream will have 2 channels 
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
WM8960Stream out; 
StreamCopy copier(out, sound);                             // copies sound into i2s

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  while(!Serial);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup wire 
  Wire.begin();
  Wire.setClock(10000);


  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.sample_rate = sample_rate; 
  config.channels = channels;
  config.bits_per_sample = 16;
  config.wire = &Wire;
  // use custom i2s pins to avoid any conflicts with i2c
  config.pin_bck = 25;
  config.pin_ws = 26;
  config.pin_data = 27;

  if (!out.begin(config)){
    stop();
  }

  // Setup sine wave
  sineWave.begin(channels, sample_rate, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
