/**
 * @file communications-ble-server-send.ino
 * @author Phil Schatzmann
 * @brief Sending audio via BLE: The server acts as audio source
 * @version 0.1
 * @date 2022-11-04
 *
 * @copyright Copyright (c) 2023
 */

#include "AudioTools.h"
#include "AudioCodecs/CodecADPCM.h" // https://github.com/pschatzmann/adpcm
#include "Sandbox/BLE/AudioBLE.h"

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave);  // Stream generated from sine wave
AudioBLEServer ble;
ADPCMEncoder adpcm(AV_CODEC_ID_ADPCM_IMA_WAV);
EncodedAudioStream encoder(&ble, &adpcm);
StreamCopy copier(encoder, sound);

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  sineWave.begin(info, N_B4);
  
  encoder.begin(info);

  //ble.setAudioInfoActive(true);
  ble.begin("ble-send");
}

void loop() { 
  if (ble.availableForWrite()>0) copier.copy();
}