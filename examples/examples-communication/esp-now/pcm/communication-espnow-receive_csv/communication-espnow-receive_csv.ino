/**
 * @file example-serial-receive_csv.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via ESPNow
 * @version 0.1
 * @date 2022-03-09
 * 
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include "Communication/ESPNowStream.h"

ESPNowStream now;
const char *peers[] = {"A8:48:FA:0B:93:02"};

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  auto cfg = now.defaultConfig();
  cfg.mac_address = "A8:48:FA:0B:93:01";
  now.begin(cfg);
  now.addPeers(peers);

  Serial.println("Receiver started...");
}

void loop() { 
  int16_t sample;
  if (now.readBytes((uint8_t*)&sample,2)){
    Serial.println(sample); 
  }  
}