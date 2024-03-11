/**
 * @file communication-container-binary-meta.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> audiokit (i2s)
 * We demonstrate how to pass any type of metadata from the encoder to the
 * decoder
 * @version 0.1
 * @date 2023-05-18
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "AudioTools.h"
#include "AudioCodecs/CodecOpus.h"
#include "AudioCodecs/ContainerBinary.h"
#include "AudioLibs/AudioBoardStream.h"

struct MetaData {
  MetaData(const char* typeArg, float vol) {
    volume = vol;
    strncpy(type, typeArg, 5);
  }
  MetaData(MetaData* data) {
    volume = data->volume;
    strncpy(type, data->type, 5);
  }

  void log() {
    Serial.print("====> ");
    Serial.print(type);
    Serial.print(": volume ");
    Serial.println(volume);
  }

 protected:
  float volume;
  char type[5];
};

void metaCallback(uint8_t* data, int len, void*ref) {
  assert(sizeof(MetaData)==len);
  MetaData meta((MetaData*)data);
  meta.log();
}

AudioInfo info(8000, 1, 16);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
AudioBoardStream out(AudioKitEs8388V1);
BinaryContainerDecoder cont_dec(new OpusAudioDecoder());
BinaryContainerEncoder cont_enc(new OpusAudioEncoder());
EncodedAudioStream decoder(&out, &cont_dec);
EncodedAudioStream encoder(&decoder, &cont_enc);StreamCopy copier(encoder, sound);
MetaData meta{"opus", 0.5};

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

  // start decoder
  decoder.begin(info);

  // start encoder
  encoder.begin(info);

  // receive meta data 
  cont_dec.setMetaCallback(metaCallback);
  
  // write meta data
  cont_enc.writeMeta((const uint8_t*)&meta, sizeof(MetaData));

  Serial.println("Test started...");
}

void loop() { copier.copy(); }