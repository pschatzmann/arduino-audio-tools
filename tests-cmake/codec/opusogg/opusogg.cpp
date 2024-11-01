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
#include "AudioTools/AudioCodecs/CodecOpusOgg.h"

int application = OPUS_APPLICATION_AUDIO; // Opus application
AudioInfo info(24000, 1, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
CsvOutput<int16_t> out(Serial);   // Output of sound on desktop 
OpusOggEncoder enc;
OpusOggDecoder dec;
EncodedAudioStream decoder(&out, &dec); // encode and write 
EncodedAudioStream encoder(&decoder, &enc); // encode and write 
StreamCopy copier(encoder, sound);     

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Setup output
  auto cfgo = out.defaultConfig();
  cfgo.copyFrom(info);
  out.begin(cfgo);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // Opus decoder needs to know the audio info
  decoder.begin(info);

  // configure and start encoder
  enc.config().application = application;
  encoder.begin(info);

  Serial.println("Test started...");
}


void loop() { 
  copier.copy();
}