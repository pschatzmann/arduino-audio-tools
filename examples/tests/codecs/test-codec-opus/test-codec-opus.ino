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
#include "AudioLibs/AudioBoardStream.h"
#include "AudioCodecs/CodecOpus.h"

AudioInfo info(24000, 1, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
AudioBoardStream out(AudioKitEs8388V1);
OpusAudioDecoder dec;
OpusAudioEncoder enc;
EncodedAudioStream decoder(&out, &dec); // encode and write 
EncodedAudioStream encoder(&decoder, &enc); // encode and write 
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

  // Opus encoder and decoder need to know the audio info
  decoder.begin(info);
  encoder.begin(info);

  // configure additinal parameters
  // auto &enc_cfg = enc.config()
  // enc_cfg.application = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
  // enc_cfg.frame_sizes_ms_x2 = OPUS_FRAMESIZE_20_MS;
  // enc_cfg.complexity = 5;

  Serial.println("Test started...");
}


void loop() { 
  copier.copy();
}