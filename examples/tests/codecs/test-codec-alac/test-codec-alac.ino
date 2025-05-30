/**
 * @file test-codec-alac.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> audiokit (i2s)
 * @version 0.1
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecALAC.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

SET_LOOP_TASK_STACK_SIZE(16*1024); // 16KB

AudioInfo info(8000, 1, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
AudioBoardStream out(AudioKitEs8388V1);
//I2SStream out; 
//CsvOutput<int16_t> out(Serial);
EncoderALAC enc_alac(1024 * 4);
DecoderALAC dec_alac;
EncodedAudioStream decoder(&out, &dec_alac); // encode and write
EncodedAudioStream encoder(&decoder, &enc_alac); // encode and write
StreamCopy copier(encoder, sound);     

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Debug);

  // start I2S
  Serial.println("starting I2S...");
  auto cfgi = out.defaultConfig(TX_MODE);
  cfgi.copyFrom(info);
  out.begin(cfgi);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // start encoder
  encoder.begin(info);

  // start decoder
  decoder.begin();
  
  // copy config from encoder to decoder
  dec_alac.writeCodecInfo(enc_alac.config());

  Serial.println("Test started...");
}


void loop() { 
  copier.copy();
}