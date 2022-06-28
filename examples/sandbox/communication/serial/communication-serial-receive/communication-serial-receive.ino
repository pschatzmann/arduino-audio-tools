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
//#include "AudioLibs/AudioKit.h"
#define RXD2 16
#define TXD2 17

uint16_t sample_rate = 44100;
uint8_t channels = 1;  // The stream will have 1 channel
I2SStream out; // or use AudioKitStream
StreamCopy copier(out, Serial2);     

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Error);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.sample_rate = sample_rate; 
  config.channels = channels;
  config.bits_per_sample = 16;
  out.begin(config);

  // Start Serial input
  Serial2.begin(1000000, SERIAL_8N1, RXD2, TXD2);

  Serial.println("started...");
}

void loop() { 
    copier.copy();
}