/**
 * @file example-serial-receive.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via IP and writing to I2S
 * @version 0.1
 * @date 2022-03-09
 * 
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include <WiFi.h>

uint16_t sample_rate = 16000;
uint16_t port = 8000;
uint8_t channels = 1;  // The stream will have 2 channels
WiFiServer server(port);
WiFiClient client; 
I2SStream out; 
MeasuringStream outTimed(out);
StreamCopy copier(outTimed, client);     
const char *ssid = "ssid";
const char *password = "password";

void connectWifi(){
  // connect to WIFI
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println();
  Serial.println(WiFi. localIP());

  // Performance Hack              
  esp_wifi_set_ps(WIFI_PS_NONE);
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  connectWifi();

  // start server
  server.begin();

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.sample_rate = sample_rate; 
  config.channels = channels;
  config.bits_per_sample = 16;
  config.buffer_size = 512;
  config.buffer_count = 6;
  out.begin(config);

  Serial.println("started...");
}

void loop() { 
  // get a new connection if necessary
  if (!client){
    client = server.available();  
  }
  // copy data if we are connected
  if (client.connected()){
    copier.copy();
  }
}
