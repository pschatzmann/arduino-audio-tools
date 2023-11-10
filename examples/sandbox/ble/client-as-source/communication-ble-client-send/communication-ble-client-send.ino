/**
 * @file communications-ble-client-send.ino
 * @author Phil Schatzmann
 * @brief Sending audio via BLE: the client acts as audio source
  * Please not that the thruput in this scenario is very limited!

 * @version 0.1
 * @date 2022-11-04
 *
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include "AudioCodecs/CodecADPCM.h" // https://github.com/pschatzmann/adpcm
#include "Sandbox/BLE/AudioBLE.h"

AudioInfo info(16000, 1, 16);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave); 
Throttle throttle(sound);
AudioBLEClient ble;
ADPCMEncoder adpcm(AV_CODEC_ID_ADPCM_IMA_WAV);
EncodedAudioStream encoder(&ble, &adpcm);
StreamCopy copier(encoder, throttle);

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  sineWave.begin(info, N_B4);
  throttle.begin(info);
  encoder.begin(info);
  ble.begin("ble-send", 60 * 10);

  Serial.println("started...");
}

void loop() { copier.copy(); }