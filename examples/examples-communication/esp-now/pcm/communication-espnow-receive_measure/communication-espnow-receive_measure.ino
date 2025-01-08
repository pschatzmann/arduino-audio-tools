/**
 * @file example-espnow-receive_measure.ino
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

ESPNowStream now;
MeasuringStream out;
StreamCopy copier(out, now);     
const char *peers[] = {"A8:48:FA:0B:93:02"};

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  auto cfg = now.defaultConfig();
  cfg.mac_address = "A8:48:FA:0B:93:01";
  now.begin(cfg);
  now.addPeers(peers);

  // start I2S
  Serial.println("starting Out...");
  out.begin();

  Serial.println("Receiver started...");
}

void loop() { 
  copier.copy();
}