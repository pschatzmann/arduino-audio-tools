/**
 * @file example-serial-send.ino
 * @author Phil Schatzmann
 * @brief Sending audio over serial bluetooth
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
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave);                      // Stream generated from sine wave
BluetoothSerial SerialBT;
StreamCopy copier(SerialBT, sound);  // copies sound into i2s

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  SerialBT.begin("ESP32Sender"); //Bluetooth device name
  while(!SerialBT.connected()){
    SerialBT.connect("ESP32Receiver");
    Serial.print(".");
    delay(1000);
  }
  Serial.println();

  // Setup sine wave
  sineWave.begin(channels, sample_rate, N_B4);
  Serial.println("started...");
}

void loop() { 
    copier.copy();
}