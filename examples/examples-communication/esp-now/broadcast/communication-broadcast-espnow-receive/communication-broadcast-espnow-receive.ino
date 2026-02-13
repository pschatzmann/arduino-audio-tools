/**
 * @file communications-broadcast-espnow-receive.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via ESPNow
 * @version 0.1
 * @date 2026-01-30
 * 
 * @copyright Copyright (c) 2026
 */

#include "AudioTools.h"
#include "AudioTools/Communication/ESPNowStream.h"

AudioInfo info(8000, 1, 16);
ESPNowStream now;
MeasuringStream now1(now);

I2SStream out; 
StreamCopy copier(out, now1);     

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  auto cfg = now.defaultConfig();
  // start receiving when 50% of the buffer is full
  cfg.start_read_threshold_percent = 50;
  cfg.mac_address = "A8:48:FA:0B:93:01";
  now.begin(cfg);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info); 
  out.begin(config);

  Serial.println("Receiver started...");
}

void loop() { 
  copier.copy();
}