/**
 * @file example-udp-receive.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via udp
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
I2SStream out; 
UDPStream udp; 
const int udpPort = 7000;
StreamCopy copier(out, udp);     
const char* ssid="SSID";
const char* password="password";

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // connect to WIFI
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println();


  udp.begin(port);

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