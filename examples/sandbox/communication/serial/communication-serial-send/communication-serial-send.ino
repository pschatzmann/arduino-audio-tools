/**
 * @file example-serial-send.ino
 * @author Phil Schatzmann
 * @brief Sending audio over serial
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

uint16_t sample_rate = 44100;
uint8_t channels = 1;  // The stream will have 1 channel
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);                      // Stream generated from sine wave
StreamCopy copier(Serial2, sound);  // copies sound into i2s

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // Note the format for setting a serial port is as follows:
  // Serial2.begin(baud-rate, protocol, RX pin, TX pin);
  Serial2.begin(1000000, SERIAL_8N1, RXD2, TXD2);

  // Setup sine wave
  sineWave.begin(channels, sample_rate, N_B4);
  Serial.println("started...");
}

void loop() { 
    copier.copy();
}