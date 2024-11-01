/**
 * @file test-codec-adpcm.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> PortAudioStream
 * @version 0.1
 * @date 2022-04-30
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecADPCM.h" // https://github.com/pschatzmann/adpcm
#include "AudioTools/AudioLibs/PortAudioStream.h"

AudioInfo info(16000, 2, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
//I2SStream out; 
PortAudioStream out; 
//CsvOutput<int16_t> out(Serial);
EncodedAudioStream decoder(&out, new ADPCMDecoder(AV_CODEC_ID_ADPCM_IMA_WAV)); // encode and write
EncodedAudioStream encoder(&decoder, new ADPCMEncoder(AV_CODEC_ID_ADPCM_IMA_WAV)); // encode and write
StreamCopy copier(encoder, sound);     

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // start I2S
  Serial.println("starting I2S...");
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
