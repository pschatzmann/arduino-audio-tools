/**
 * @file example-udp-send.ino
 * @author Phil Schatzmann
 * @brief Sending audio over udp. Because the SineWaveGenerator is providing the data as fast 
 * as possible we need to throttle the speed.
 * @version 0.1
 * @date 2022-03-09
 *
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include "AudioTools/Communication/UDPStream.h"

const char *ssid = "ssid";
const char *password = "password";
AudioInfo info(22000, 1, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave);  // Stream generated from sine wave
UDPStream udp(ssid, password); 
Throttle throttle(udp);
IPAddress udpAddress(192, 168, 1, 255);
const int udpPort = 7000;
StreamCopy copier(throttle, sound);  // copies sound into i2s


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // Define udp address and port
  udp.begin(udpAddress, udpPort);

  // Define Throttle
  auto cfg = throttle.defaultConfig();
  cfg.copyFrom(info);
  //cfg.correction_ms = 0;
  throttle.begin(cfg);

  Serial.println("started...");
}

void loop() { 
    copier.copy();
}