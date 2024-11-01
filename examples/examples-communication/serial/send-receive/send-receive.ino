/**
 * @file send-receive.ino
 * @author Phil Schatzmann
 * @brief Sending and receiving audio via Serial. You need to connect the RX pin
 * with the TX pin!
 * The sine wave generator is providing the data as fast as possible, therefore we 
 * throttle on the sending side to prevent that the receiver is getting the data
 * too fast. 
 *
 * @version 0.1
 * @date 2022-03-09
 *
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
// #include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(22000, 1, 16);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
I2SStream out; // or AnalogAudioStream, AudioBoardStream etc
auto &serial = Serial2;
Throttle throttle(serial);
StreamCopy copierOut(throttle, sound, 256);  // copies sound into Serial
StreamCopy copierIn(out, serial, 256);     // copies sound from Serial

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Note the format for setting a serial port is as follows:
  // Serial.begin(baud-rate, protocol, RX pin, TX pin);
  Serial2.begin(3000000, SERIAL_8N1 );

  // Setup sine wave
  sineWave.begin(info, N_B4);
  throttle.begin(info);

  // start I2S
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  out.begin(config);

  // better visibility in logging
  copierOut.setLogName("out");
  copierIn.setLogName("in");

}

void loop() {
  // copy to serial
  copierOut.copy();
  // copy from serial
  copierIn.copy();
}