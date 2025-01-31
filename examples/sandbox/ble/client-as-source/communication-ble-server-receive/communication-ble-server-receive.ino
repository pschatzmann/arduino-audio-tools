/**
 * @file communications-ble-server-receive.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via BLE: the server acts as audio sink
 * Please not that the thruput in this scenario is very limited!
 * @version 0.1
 * @date 2022-11-04
 *
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecADPCM.h" // https://github.com/pschatzmann/adpcm
#include "AudioTools/Sandbox/BLE/AudioBLE.h"
//#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(16000, 1, 16);
ADPCMDecoder adpcm(AV_CODEC_ID_ADPCM_IMA_WAV);
I2SStream i2s; // or AudioBoardStream ...
EncodedAudioStream decoder(&i2s, &adpcm);
AudioBLEServer ble;
StreamCopy copier(decoder, ble);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);
 
  decoder.begin(info);
  
  auto cfg = i2s.defaultConfig();
  cfg.copyFrom(info);
  i2s.begin(cfg);

  ble.begin("ble-receive");
}

void loop() { 
  copier.copy();
}