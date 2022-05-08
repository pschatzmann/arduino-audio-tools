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
#include "AudioCodecs/CodecLC3.h"
#include "AudioLibs/AudioKit.h"

uint16_t sample_rate = 24000; // (must be 8000,16000,24000,32000,48000)
uint8_t channels = 1;  // The stream will have 2 channels
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
AudioKitStream out;
//I2SStream out; 
//CsvStream<int16_t> out(Serial,channels);
EncodedAudioStream decoder(&out, new LC3Decoder()); // encode and write
EncodedAudioStream encoder(&decoder, new LC3Encoder()); // encode and write
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
  sineWave.begin(cfgs, 400);

  // start decoder
  decoder.begin(cfgs);// LC3 does not provide audio info from source

  // start encoder
  encoder.begin(cfgs);


  Serial.println("Test started...");
}


void loop() { 
  copier.copy();
}