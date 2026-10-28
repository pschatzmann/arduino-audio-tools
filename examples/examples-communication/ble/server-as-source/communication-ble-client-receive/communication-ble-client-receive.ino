/**
 * @file communications-ble-client-receive.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via BLE: The client acts as audio sink and prints
 * the decoded samples as CSV (replace CsvOutput with I2SStream or
 * AudioBoardStream to hear the audio).
 * This scenario works amazingly well!
 * @version 0.1
 * @date 2022-11-04
 *
 * @copyright Copyright (c) 2023
 */


#include "AudioTools.h"
//#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioCodecs/CodecADPCM.h" // https://github.com/pschatzmann/adpcm
#include "AudioTools/Communication/BLESerial.h"

AudioInfo info(44100, 2, 16);
BLESerialClient ble;
CsvOutput<int16_t> out(Serial); // or I2SStream, AudioBoardStream ...
ADPCMDecoder adpcm(AV_CODEC_ID_ADPCM_IMA_WAV);
EncodedAudioStream decoder(&out, &adpcm);
StreamCopy copier(decoder, ble);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // start BLE client - wait at most 10 minutes
  ble.begin("ble-receive", 60*10);

  // start decoder
  decoder.begin(info);

  // start output
  out.begin(info);

  Serial.println("started...");
}

void loop() {
  if (ble)
    copier.copy();
}
