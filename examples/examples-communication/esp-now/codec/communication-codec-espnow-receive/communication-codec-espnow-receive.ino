/**
 * @file example-serial-receive.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via ESPNow and decoding data to I2S 
 * @version 0.1
 * @date 2022-03-09
 * 
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include "AudioTools/Communication/ESPNowStream.h"
#include "AudioTools/AudioCodecs/CodecSBC.h"
// #include "AudioTools/AudioLibs/AudioBoardStream.h"


ESPNowStream now;
I2SStream out;  // or AudioBoardStream
EncodedAudioStream decoder(&out, new SBCDecoder(256)); // decode and write to I2S - ESP Now is limited to 256 bytes
StreamCopy copier(decoder, now);     
const char *peers[] = {"A8:48:FA:0B:93:02"};

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // setup esp-now
  auto cfg = now.defaultConfig();
  cfg.mac_address = "A8:48:FA:0B:93:01";
  now.begin(cfg);
  now.addPeers(peers);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  out.begin(config);

  // start decoder
  decoder.begin();


  Serial.println("Receiver started...");
}

void loop() { 
  copier.copy();
}