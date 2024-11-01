/**
 * @file test-codec-adpcm.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> PortAudioStream
 * We can select between PCM and ADPCM
 * @version 0.1
 * @date 2022-04-30
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecWAV.h" 
#include "AudioTools/AudioCodecs/CodecADPCM.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"

#define USE_ADPCM true

AudioInfo info(16000, 2, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
//I2SStream out; 
//PortAudioStream out; 
CsvOutput<int16_t> out(Serial);
#if USE_ADPCM
ADPCMDecoder adpcm_decoder(AV_CODEC_ID_ADPCM_IMA_WAV); 
ADPCMEncoder adpcm_encoder(AV_CODEC_ID_ADPCM_IMA_WAV); 
EncodedAudioStream decoder(&out, new WAVDecoder(adpcm_decoder, AudioFormat::ADPCM)); // encode and write
EncodedAudioStream encoder(&decoder, new WAVEncoder(adpcm_encoder, AudioFormat::ADPCM)); // encode and write
#else
EncodedAudioStream decoder(&out, new WAVDecoder()); // encode and write
EncodedAudioStream encoder(&decoder, new WAVEncoder()); // encode and write
#endif
StreamCopy copier(encoder, sound);     

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Debug);

  // start I2S
  Serial.println("starting Output...");
  auto cfgi = out.defaultConfig(TX_MODE);
  cfgi.copyFrom(info);
  out.begin(cfgi);

  // Setup sine wave
  auto cfgs = sineWave.defaultConfig();
  cfgs.copyFrom(info);
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
