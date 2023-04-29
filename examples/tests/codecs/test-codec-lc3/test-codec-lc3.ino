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

AudioInfo info(24000, 1, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
AudioKitStream out;
//I2SStream out; 
//CsvOutput<int16_t> out(Serial,channels);
EncodedAudioStream decoder(&out, new LC3Decoder()); // encode and write
EncodedAudioStream encoder(&decoder, new LC3Encoder()); // encode and write
StreamCopy copier(encoder, sound);     

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // start I2S
  Serial.println("starting I2S...");
  auto cfgi = out.defaultConfig(TX_MODE);
  cfgi.copyFrom(info);
  out.begin(cfgi);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // start decoder
  decoder.begin(info);// LC3 does not provide audio info from source

  // start encoder
  encoder.begin(info);


  Serial.println("Test started...");
}


void loop() { 
  copier.copy();
}