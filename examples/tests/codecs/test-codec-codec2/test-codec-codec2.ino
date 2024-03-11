/**
 * @file communication-codec-test.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> csv (Serial)
 * Unfortunately codec2 needs more stack then we have available in Arduino. So we do the processing
 * in a separate task with a stack of 20k
 * 
 * @version 0.1
 * @date 2022-04-30
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioCodecs/CodecCodec2.h"
#include "AudioLibs/AudioBoardStream.h"

AudioInfo info(8000, 1, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
//CsvOutput<int16_t> out(Serial, channels); 
AudioBoardStream out(AudioKitEs8388V1);
EncodedAudioStream decoder(&out, new Codec2Decoder()); // encode and write
EncodedAudioStream encoder(&decoder, new Codec2Encoder()); // encode and write
StreamCopy copier(encoder, sound);     


void loop1(void*) {
  // setup decoder
  decoder.begin(info);
  // setup encoder
  encoder.begin(info);
  // processing loop
  while(true){
    copier.copy();
    delay(1);
  }
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  auto cfg = out.defaultConfig();
  cfg.copyFrom(info);
  out.begin(cfg);

  int stack = 20000;
  xTaskCreate(loop1,"loopTask", stack, nullptr,1, nullptr);

  Serial.println("Test started...");
}


void loop() {
}