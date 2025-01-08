/**
 * @file example-lora-receive_measure.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via LoRa with max speed to measure thruput.
 * @version 0.1
 * @date 2023-11-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "AudioTools/Communication/LoRaStream.h"

LoRaStream lora;
MeasuringStream out;  // or CSVStream<uint8_t>
StreamCopy copier(out, lora);     

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  while(!Serial);

  auto cfg = lora.defaultConfig();
  lora.begin(cfg);

  // start output
  Serial.println("starting Out...");
  out.begin();

  Serial.println("Receiver started...");
}

void loop() { 
  copier.copy();
}