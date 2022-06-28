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
#define RXD2 16
#define TXD2 17

void setup() {
  Serial.begin(1000000);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // Start Serial input
  Serial2.begin(1000000, SERIAL_8N1, RXD2, TXD2);

  Serial.println("started...");
}

void loop() { 
  if (Serial2.available()>=2){
    int16_t sample;
    if (Serial2.readBytes((uint8_t*)&sample,2)){
      Serial.println(sample);
    }
  }
}