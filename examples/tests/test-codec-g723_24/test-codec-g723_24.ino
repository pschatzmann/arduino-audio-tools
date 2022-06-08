/**
 * @file communication-codec-test.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> audiokit (i2s)
 * @version 0.1
 * @date 2022-04-30
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioCodecs/CodecG7xx.h"
#include "AudioLibs/AudioKit.h"

uint16_t sample_rate = 8000;
uint8_t channels = 1;  // The stream will have 2 channels
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
AudioKitStream out; 
EncodedAudioStream decoder(&out, new G723_24Decoder()); // encode and write
EncodedAudioStream encoder(&decoder, new G723_24Encoder()); // encode and write
StreamCopy copier(encoder, sound);     

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // start I2S
  Serial.println("starting I2S...");
  auto cfgi = out.defaultConfig(TX_MODE);
  cfgi.sample_rate = sample_rate;
  cfgi.channels = channels;
  cfgi.bits_per_sample = 16;
  out.begin(cfgi);

  // Setup sine wave
  auto cfgs = sineWave.defaultConfig();
  cfgs.sample_rate = sample_rate;
  cfgs.channels = channels;
  cfgs.bits_per_sample = 16;
  sineWave.begin(cfgs, N_B4);

  // start decoder
  decoder.begin();

  // start encoder
  encoder.begin(cfgs);

  Serial.println("Test started...");
}


void loop() { 
  copier.copy();
}