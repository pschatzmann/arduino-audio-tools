/**
 * @file example-serial-receive.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via ESPNow
 * @version 0.1
 * @date 2022-03-09
 * 
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include "AudioLibs/Communication.h"

uint16_t sample_rate = 8000;
uint8_t channels = 1;  // The stream will have 2 channels
ESPNowStream now;
MeasuringStream now1(now);

I2SStream out; 
StreamCopy copier(out, now1);     
const char *peers[] = {"A8:48:FA:0B:93:02"};

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  auto cfg = now.defaultConfig();
  cfg.mac_address = "A8:48:FA:0B:93:01";
  now.begin(cfg);
  now.addPeers(peers);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.sample_rate = sample_rate; 
  config.channels = channels;
  config.bits_per_sample = 16;
  out.begin(config);

  Serial.println("Receiver started...");
}

void loop() { 
  copier.copy();
}