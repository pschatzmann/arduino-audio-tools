/**
 * @file example-udp-receive.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via udp.
 * Because the clocks of the 2 processors are not synchronized, we might get some
 * buffer overruns or underruns. 
 * @version 0.1
 * @date 2022-03-09
 * 
 * @copyright Copyright (c) 2022
 *  */

#include "AudioTools.h"
#include "Communication/UDPStream.h"
// #include "AudioLibs/AudioBoardStream.h"

const char* ssid="SSID";
const char* password="password";
AudioInfo info(22000, 1, 16);
UDPStream udp(ssid, password); 
const int udpPort = 7000;
I2SStream out; // or ony other e.g. AudioBoardStream
StreamCopy copier(out, udp);     

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start UDP receive
  udp.begin(udpPort);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.buffer_size = 1024;
  config.buffer_count = 20;
  out.begin(config);

  Serial.println("started...");
}

void loop() { 
    copier.copy();
}