/**
 * @file communication-container-binary.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> audiokit (i2s)
 * @version 0.1
 * @date 2022-04-30
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerBinary.h"

AudioInfo info(8000,1,16);
SineWaveGeneratorT<int16_t> sineWave( 32000);  // subclass of SoundGeneratorT with max amplitude of 32000
GeneratedSoundStreamT<int16_t> sound( sineWave); // Stream generated from sine wave
CsvOutput out(Serial);
EncodedAudioStream decoder(&out,new BinaryContainerDecoder()); // encode and write
EncodedAudioStream encoder(&out,new BinaryContainerEncoder()); // encode and write
StreamCopy copier(encoder, sound);     

void setup() {
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // start 
  Serial.println("starting...");
  auto cfgi = out.defaultConfig(TX_MODE);
  cfgi.copyFrom(info);
  out.begin(cfgi);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // start decoder
  decoder.begin(info);

  // start encoder
  encoder.begin(info);

  Serial.println("Test started...");
}


void loop() { 
  copier.copy();
}