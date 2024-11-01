/**
 * @file send-8bit-receive.ino
 * @author Phil Schatzmann
 * @brief Sending and receiving audio via Serial. You need to connect the RX pin
 * with the TX pin!
 * 
 * We send 8bit data over the wire, so any (small) data loss will not be audible
 *
 * @version 0.1
 * @date 2023-11-25
 *
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
// #include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 2, 16);
I2SStream out; // or AnalogAudioStream, AudioBoardStream etc
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
auto &serial = Serial2;
EncoderL8 enc;
DecoderL8 dec;
EncodedAudioStream enc_stream(&serial, &enc);
EncodedAudioStream dec_stream(&out, &dec);
Throttle throttle(enc_stream);
StreamCopy copierOut(throttle, sound, 256);  // copies sound into Serial
StreamCopy copierIn(dec_stream, serial, 256);     // copies sound from Serial

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Note the format for setting a serial port is as follows:
  // Serial.begin(baud-rate, protocol, RX pin, TX pin);
  Serial2.begin(3000000, SERIAL_8N1);

  sineWave.begin(info, N_B4);
  throttle.begin(info);
  enc_stream.begin(info);
  dec_stream.begin(info);

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