/**
 * @file generator-am_sender3.ino
 * @author Phil Schatzmann
 * @brief We build a AM radio with the help of the audio tools library RFStream class
 * @version 0.1
 * @date 2022-06-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "Experiments/RFStream.h"

uint16_t sample_rate=44100;
uint8_t channels = 1;                                      // The stream will have 2 channels 
SineFromTable<int16_t> sineWave(32000);                    // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
RfStream out; 
StreamCopy copier(out, sound, 100);                        // copies sound into i2s

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start RFStream
  Serial.println("starting RFStream...");
  auto config = out.defaultConfig();
  config.sample_rate = sample_rate; 
  config.channels = channels;
  config.bits_per_sample = 16;
  config.rf_frequency = 835000; // carrier frequency
  // config.modulation = MOD_AM;
  // config.resample_scenario = UPSAMPLE;
  out.begin(config);

  // Setup sine wave
  sineWave.begin(channels, sample_rate, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
