/**
 * @file send-hdlc-receive.ino
 * @author Phil Schatzmann
 * @brief Sending and receiving audio via Serial. You need to connect the RX pin
 * with the TX pin!
 * The sine wave generator is providing the data as fast as possible, therefore we 
 * throttle on the sending side to prevent that the receiver is getting the data
 * too fast.  
 * 
 * To make sure that we transmit only valid data we use HDLC.
 *
 * @version 0.1
 * @date 2022-03-09
 *
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include "Communication/HDLCStream.h"
// #include "AudioLibs/AudioBoardStream.h"

AudioInfo info(8000, 1, 16);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
Throttle throttle(sound);
I2SStream out;
HDLCStream hdlc_enc(Serial, 256);
HDLCStream hdlc_dec(Serial, 256);
StreamCopy copierOut(hdlc_enc, throttle, 256); 
StreamCopy copierIn(out, hdlc_dec, 256);     

void setup() {
  Serial2.begin(115200);
  AudioLogger::instance().begin(Serial2, AudioLogger::Warning);
  hdlc_enc.begin();
  hdlc_dec.begin();

  // Note the format for setting a serial port is as follows:
  // Serial.begin(baud-rate, protocol, RX pin, TX pin);
  Serial.begin(1000000, SERIAL_8N1);

  // Setup sine wave
  sineWave.begin(info, N_B4);
  throttle.begin(info);

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