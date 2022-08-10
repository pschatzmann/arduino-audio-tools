/**
 * @file streams-generator-vs1053
 * @author Phil Schatzmann
 * @brief Generate Test Tone
 * @version 0.1
 * @date 2022-08-10
 * 
 * @copyright Copyright (c) 2022
 * 
 */

// install https://github.com/baldram/ESP_VS1053_Library.git

#include "AudioTools.h"
#include "AudioLibs/VS1053Stream.h"

// example pins for an ESP32
#define VS1053_CS     5
#define VS1053_DCS    16
#define VS1053_DREQ   4

uint16_t sample_rate=44100;
uint8_t channels = 2;                                      // The stream will have 2 channels 
uint8_t bits_per_sample = 16;                              // 2 bytes 
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
VS1053Stream vs1053(VS1053_CS,VS1053_DCS, VS1053_DREQ);    // final output
EncodedAudioStream out(&vs1053, new WAVEncoder());         // output is WAV file
StreamCopy copier(out, sound); // copy sound to decoder


void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // Setup sine wave
  sineWave.begin(channels, sample_rate, N_A4);

  // setup output
  auto cfg = out.defaultConfig();
  cfg.sample_rate = sample_rate;
  cfg.channels = channels;
  cfg.bits_per_sample = bits_per_sample;
  out.begin(cfg);

  // setup vs1053
  vs1053.begin();
}

void loop(){
  copier.copy();
}
