/**
 * @file test-codec-opus.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> audiokit (i2s)
 * @version 0.1
 * @date 2022-04-30
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioCodecs/CodecOpus.h"

int sample_rate = 24000;
int channels = 1;  

SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
AudioKitStream out; 
OpusAudioEncoder enc;
EncodedAudioStream decoder(&out, new OpusAudioDecoder()); // encode and write 
EncodedAudioStream encoder(&decoder, &enc); // encode and write 
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

  // Opus encoder and decoder need to know the audio info
  decoder.begin(cfgs);
  encoder.begin(cfgs);

  // configure additinal parameters
  //enc.config().application = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
  //enc.config().frame_sizes_ms_x2 = OPUS_FRAMESIZE_20_MS;
  //enc.config().complexity = 5;

  Serial.println("Test started...");
}


void loop() { 
  copier.copy();
}