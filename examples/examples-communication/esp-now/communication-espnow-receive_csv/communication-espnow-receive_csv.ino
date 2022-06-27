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
#include "AudioLibs/Communication.h"

uint16_t sample_rate = 10000;
uint8_t channels = 1;  // The stream will have 2 channels
ESPNowStream now;
CsvStream<int16_t> out(Serial1, channels); 
StreamCopy copier(out, now1);     
const char *peers[] = {"A8:48:FA:0B:93:02"};

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  auto cfg = now.defaultConfig();
  cfg.mac_address = "A8:48:FA:0B:93:01";
  now.begin(cfg);
  now.addPeers(peers);

  out.begin();

  Serial.println("Receiver started...");
}

void loop() { 
  copier.copy();
}