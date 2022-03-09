/**
 * @file example-serial-receive.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via ESPNow
 * @version 0.1
 * @date 2022-03-09
 * 
 * @copyright Copyright (c) 2022
 * 
 * ATTENTION: DRAFT - not tested yet
 */

#include "AudioTools.h"
#include "AudioLibs/Communication.h"

uint16_t sample_rate = 44100;
uint8_t channels = 2;  // The stream will have 2 channels
ESPNowStream now;
I2SStream out; 
StreamCopy copier(out, now);     
const char *ssid = "ssid";
const char *password = "password";
const char *peers[] = {"A8:48:FA:0B:93:40"};

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  now.addPeers(peers);
  now.begin(ssid, password);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.sample_rate = sample_rate; 
  config.channels = channels;
  config.bits_per_sample = 16;
  out.begin(config);

  Serial.println("started...");
}

void loop() { 
    copier.copy();
}