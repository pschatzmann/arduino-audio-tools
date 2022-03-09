/**
 * @file example-serial-receive.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via serial
 * @version 0.1
 * @date 2022-03-09
 * 
 * @copyright Copyright (c) 2022
 * 
 * ATTENTION: DRAFT - not tested yet
 */

#include "AudioTools.h"
#include <WiFi.h>

uint16_t sample_rate = 44100;
uint8_t channels = 2;  // The stream will have 2 channels
WiFiServer server(80);
WiFiClient client; 
I2SStream out; 
StreamCopy copier(out, client);     
const char* ssid     = "yourssid";
const char* password = "yourpasswd";

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start server
  server.begin();

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
  // get a new connection
  if (!client){
    client = server.available();  
  }
  // copy data if we are connected
  if (client.connected()){
    copier.copy();
  }
}