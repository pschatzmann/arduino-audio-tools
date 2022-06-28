/**
 * @file example-serial-receive.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via serial bluetooth
 * @version 0.1
 * @date 2022-03-09
 * 
 * @copyright Copyright (c) 2022
 * 
 * ATTENTION: DRAFT - not tested yet
 */

#include "AudioTools.h"
#include "BluetoothSerial.h"

uint16_t sample_rate = 44100;
uint8_t channels = 2;  // The stream will have 2 channels
I2SStream out; 
BluetoothSerial SerialBT;
StreamCopy copier(out, SerialBT);     

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  SerialBT.begin("ESP32Receiver"); //Bluetooth device name
  while(!SerialBT.connected()){
    //SerialBT.connect("ESP32Receiver");
    Serial.print(".");
    delay(1000);
  }
  Serial.println();

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.sample_rate = sample_rate; 
  config.channels = channels;
  config.bits_per_sample = 16;
  out.begin(config);

  Serial.println("started...");
}

void loop() { 
    copier.copy();
}