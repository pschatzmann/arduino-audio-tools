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
#include "AudioLibs/AudioKit.h"

uint16_t sample_rate = 8000;
uint8_t channels = 1;  // The stream will have 2 channels
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
CsvStream<int16_t> out(Serial, channels); 
EncodedAudioStream decoder(&out, new Codec2Decoder()); // encode and write
EncodedAudioStream encoder(&decoder, new Codec2Encoder()); // encode and write
StreamCopy copier(encoder, sound);     


void loop1(void*) {
  // setup decoder
  decoder.begin();
  // setup encoder
  encoder.begin();
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
  auto cfgs = sineWave.defaultConfig();
  cfgs.sample_rate = sample_rate;
  cfgs.channels = channels;
  cfgs.bits_per_sample = 16;
  sineWave.begin(cfgs, N_B4);

  int stack = 20000;
  xTaskCreate(loop1,"loopTask", stack, nullptr,1, nullptr);

  Serial.println("Test started...");
}


void loop() {
}