/**
 * @file example-serial-receive_measure.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via ESPNow with max speed to measure thruput
 * @version 0.1
 * @date 2022-03-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "AudioTools/Communication/ESPNowStream.h"
#include "AudioTools/AudioCodecs/CodecSBC.h"

ESPNowStream now;
MeasuringStream out(1000, &Serial);
EncodedAudioStream decoder(&out, new SBCDecoder()); // decode and write to I2S
StreamCopy copier(decoder, now);     
const char *peers[] = {"A8:48:FA:0B:93:02"};

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // start esp-now
  auto cfg = now.defaultConfig();
  cfg.mac_address = "A8:48:FA:0B:93:01";
  now.begin(cfg);
  now.addPeers(peers);

  // start output
  decoder.begin();
  out.begin();

  Serial.println("Receiver started...");
}

void loop() { 
  copier.copy();
}