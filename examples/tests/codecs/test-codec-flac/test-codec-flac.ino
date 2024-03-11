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
#include "AudioCodecs/CodecFLAC.h"
#include "AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
HexDumpOutput out(Serial);
EncodedAudioStream encoder(&out, new FLACEncoder()); // encode and write
StreamCopy copier(encoder, sound);     

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // start encoder
  encoder.begin(info);

  Serial.println("Test started...");
}


void loop() { 
  copier.copy();
}