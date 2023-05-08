/**
 * @file test-codec-opusogg.ino
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
#include "AudioCodecs/CodecOpusOgg.h"

AudioInfo info(24000, 1, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
AudioKitStream out;
//CsvOutput<int16_t> out(Serial); 
OpusOggEncoder enc;
OpusOggDecoder dec;
EncodedAudioStream decoder(&out, &dec); // encode and write 
EncodedAudioStream encoder(&decoder, &enc); // encode and write 
StreamCopy copier(encoder, sound);     

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Setup output
  auto cfgo = out.defaultConfig();
  cfgo.copyFrom(info);
  out.begin(cfgo);

  // Setup sine wave
  auto cfgs = sineWave.defaultConfig();
  cfgs.copyFrom(info);
  sineWave.begin(cfgs, N_B4);

  // Opus encoder needs to know the audio info
  encoder.begin(info);
  decoder.begin(info);

  // configure additinal parameters
  //enc.config().application = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
  //enc.config().frame_sizes_ms_x2 = OPUS_FRAMESIZE_20_MS;
  //enc.config().complexity = 5;

  Serial.println("Test started...");
}


void loop() { 
  copier.copy();
}