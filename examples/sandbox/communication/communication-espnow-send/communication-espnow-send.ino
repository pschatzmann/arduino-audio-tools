/**
 * @file example-serial-send.ino
 * @author Phil Schatzmann
 * @brief Sending audio over ESPNow
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
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
ESPNowStream now;
StreamCopy copier(now, sound);  // copies sound into i2s
const char *ssid = "ssid";
const char *password = "password";
const char *peers[] = {"A8:48:FA:0B:93:40"};

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  now.addPeers(peers);
  now.begin(ssid, password);

  // Setup sine wave
  sineWave.begin(channels, sample_rate, N_B4);
  Serial.println("started...");
}

void loop() { 
    copier.copy();
}