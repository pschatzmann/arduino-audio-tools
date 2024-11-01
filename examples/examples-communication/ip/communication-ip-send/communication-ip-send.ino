/**
 * @file example-serial-send.ino
 * @author Phil Schatzmann
 * @brief Sending audio over IP
 * @version 0.1
 * @date 2022-03-09
 *
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include <WiFi.h>

AudioInfo info(16000, 1, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave);  // Stream generated from sine wave
WiFiClient client;                  
MeasuringStream clientTimed(client);
StreamCopy copier(clientTimed, sound, 256);  // copies sound into MeasuringStream 
const char *ssid = "ssid";
const char *password = "password";
const char *client_address = "192.168.1.33"; // update based on your receive ip
uint16_t port = 8000;

void connectWifi() {
  // connect to WIFI
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println();
  Serial.println(WiFi. localIP());

  // Performance Hack              
  client.setNoDelay(true);
  esp_wifi_set_ps(WIFI_PS_NONE);
}

void connectIP() {
  if (!client.connected()){
    while (!client.connect(client_address, port)) {
      Serial.println("trying to connect...");
      delay(5000);
    }    
  }
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  connectWifi();
  // Setup sine wave
  sineWave.begin(info, N_B4);
  Serial.println("started...");
}

void loop() { 
  connectIP();  // e.g if client is shut down we try to reconnect
  copier.copy();
}