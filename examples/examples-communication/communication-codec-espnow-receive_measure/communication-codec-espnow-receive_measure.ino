/**
 * @file example-serial-receive_measure.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via ESPNow with max speed to measure thruput
 * @version 0.1
 * @date 2022-03-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "AudioLibs/Communication.h"
#include "AudioCodecs/CodecOpenAptx.h"

ESPNowStream now;
MeasuringStream out;
EncodedAudioStream decoder(&out, new OpenAptxDecoder()); // decode and write to I2S
StreamCopy copier(decoder, now);     
const char *peers[] = {"A8:48:FA:0B:93:02"};

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start esp-now
  auto cfg = now.defaultConfig();
  cfg.mac_address = "A8:48:FA:0B:93:01";
  now.begin(cfg);
  now.addPeers(peers);

  // start output
  decoder.begin();
  out.begin();

  Serial.println("Receiver started...");
}

void loop() { 
  copier.copy();
}