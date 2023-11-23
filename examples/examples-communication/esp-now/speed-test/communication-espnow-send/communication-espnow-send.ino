/**
 * @file example-serial-send.ino
 * @author Phil Schatzmann
 * @brief Sending data over ESPNow. We define a specific esp-now rate
 * @version 0.1
 * @date 2022-03-09
 *
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include "Communication/ESPNowStream.h"

ESPNowStream now;
const char *peers[] = {"A8:48:FA:0B:93:01"};
uint8_t buffer[1024] = {0};

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  auto cfg = now.defaultConfig();
  cfg.mac_address = "A8:48:FA:0B:93:02";
  cfg.rate = WIFI_PHY_RATE_24M;
  now.begin(cfg);
  now.addPeers(peers);

  Serial.println("Sender started...");
}

void loop() { 
  now.write(buffer, 1024);
}