/**
 * @file example-udp-send.ino
 * @author Phil Schatzmann
 * @brief Sending audio over udp
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
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave);  // Stream generated from sine wave
UDPStream udp; 
Throttle throttle;
IPAddress udpAddress(192, 168, 1, 255);
const int udpPort = 7000;
StreamCopy copier(udp, sound);  // copies sound into i2s
const char *ssid = "ssid";
const char *password = "password";


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
  Serial.println(WiFi.localIP());


  // Define udp address and port
  udp.begin(udpAddress, udpPort);
  auto cfg = throttle.defaultConfig();
  cfg.channels = channels;
  cfg.sample_rate = sample_rate;
  throttle.begin(cfg);

  // Setup sine wave
  sineWave.begin(channels, sample_rate, N_B4);
  Serial.println("started...");
}

void loop() { 
    throttle.startDelay();
    int bytes = copier.copy();
    throttle.delayBytes(bytes);
}