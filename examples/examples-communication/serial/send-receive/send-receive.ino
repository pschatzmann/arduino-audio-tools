/**
 * @file send-receive.ino
 * @author Phil Schatzmann
 * @brief Sending and receiving audio via Serial. You need to connect the RX pin
 * with the TX pin!
 *
 * @version 0.1
 * @date 2022-03-09
 *
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
// #include "AudioLibs/AudioKit.h"

AudioInfo info(22000, 1, 16);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
I2SStream out;                        // or AudioKitStream
StreamCopy copierOut(Serial, sound, 256);  // copies sound into Serial
StreamCopy copierIn(out, Serial, 256);     // copies sound from Serial

void setup() {
  Serial2.begin(115200);
  AudioLogger::instance().begin(Serial2, AudioLogger::Warning);

  // Note the format for setting a serial port is as follows:
  // Serial.begin(baud-rate, protocol, RX pin, TX pin);
  Serial.begin(1000000, SERIAL_8N1);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  out.begin(config);

  Serial.println("started...");
}

void loop() {
  // copy to serial
  copierOut.copy();
  // copy from serial
  copierIn.copy();
}