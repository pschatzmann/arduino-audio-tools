/**
 * @file test-codec-alac.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> audiokit (i2s)
 * @note Activate PSRAM or dicrease the frame size e.g. by adding 1024 to the constructor of the enc_alac and dec_alac 
 * @version 0.1
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecALAC.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

// SET_LOOP_TASK_STACK_SIZE(16*1024); // 16KB - not needed

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
AudioBoardStream out(AudioKitEs8388V1);
//I2SStream out; 
//CsvOutput<int16_t> out(Serial);
EncoderALAC enc_alac;
DecoderALAC dec_alac;
CodecNOP dec_nop;
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
  decoder.begin(info);
  
  // optionally copy config from encoder to decoder
  // since decoder already has audio info and frames
  // dec_alac.setCodecConfig(enc_alac.config());
  // dec_alac.setCodecConfig(enc_alac.binaryConfig());

  Serial.println("Test started...");
}


void loop() { 
  copier.copy();
}
