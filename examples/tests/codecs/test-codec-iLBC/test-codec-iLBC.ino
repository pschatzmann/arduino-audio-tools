/**
 * @file test-codec-iLBC.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> audiokit (i2s)
 * @version 0.1
 * @date 2022-04-30
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioCodecs/CodecILBC.h"
#include "AudioLibs/AudioBoardStream.h"

AudioInfo info(8000, 1, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
AudioBoardStream out(AudioKitEs8388V1);
EncodedAudioStream decoder(&out, new ILBCDecoder()); // encode and write
EncodedAudioStream encoder(&decoder, new ILBCEncoder()); // encode and write
StreamCopy copier(encoder, sound);     

void loop1(void*) {
  // start decoder
  decoder.begin(info);

  // start encoder
  encoder.begin(info);
  while(true){
    copier.copy();
  }
}

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

  int stack = 20000;
  xTaskCreate(loop1,"loopTask", stack, nullptr,1, nullptr);

  Serial.println("Test started...");
}


void loop() { 
}