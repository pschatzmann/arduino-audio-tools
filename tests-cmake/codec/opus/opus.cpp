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
#include "AudioTools/AudioCodecs/CodecOpus.h"

AudioInfo info(24000, 2, 16);
SineWaveGeneratorT<int16_t> sineWave( 32000);  // subclass of SoundGeneratorT with max amplitude of 32000
GeneratedSoundStreamT<int16_t> sound( sineWave); // Stream generated from sine wave
CsvOutput out(Serial);   // Output of sound on desktop 
OpusAudioEncoder enc;
OpusAudioDecoder dec;
EncodedAudioStream decoder(&out, &dec); // encode and write 
EncodedAudioStream encoder(&decoder, &enc); // encode and write 
StreamCopy copier(encoder, sound);     

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Debug);

  // Setup sine wave
  auto cfgs = sineWave.defaultConfig();
  cfgs.copyFrom(info);
  sineWave.begin(cfgs, N_B4);

  // Opus decoder needs to know the audio info
  decoder.begin(cfgs);

  // configure and start encoder
  enc.config().application = OPUS_APPLICATION_AUDIO;
  encoder.begin(cfgs);

  // open output
  out.begin(info);

  Serial.println("Test started...");
}


void loop() { 
  copier.copy();
}