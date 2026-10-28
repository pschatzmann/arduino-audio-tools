/**
 * @file communications-ble-server-receive.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via BLE: the server acts as audio sink and prints
 * the decoded samples as CSV (replace CsvOutput with I2SStream or
 * AudioBoardStream to hear the audio).
 * Please note that the throughput in this scenario is very limited!
 * @version 0.1
 * @date 2022-11-04
 *
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecADPCM.h" // https://github.com/pschatzmann/adpcm
#include "AudioTools/Communication/BLESerial.h"
//#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(16000, 1, 16);
ADPCMDecoder adpcm(AV_CODEC_ID_ADPCM_IMA_WAV);
CsvOutput<int16_t> out(Serial); // or I2SStream, AudioBoardStream ...
EncodedAudioStream decoder(&out, &adpcm);
BLESerialServer ble;
StreamCopy copier(decoder, ble);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);
 
  decoder.begin(info);
  
  out.begin(info);

  ble.begin("ble-receive");
}

void loop() { 
  copier.copy();
}