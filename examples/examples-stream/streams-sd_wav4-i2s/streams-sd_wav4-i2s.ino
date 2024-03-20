/**
 * @file streams-sd_wav4-i2s.ino
 * @author Phil Schatzmann
 * @brief decode WAV file with 4 channels and output it on 2 I2S ports
 * @version 0.1
 * @date 2021-96-25
 *
 * @copyright Copyright (c) 2021
 */

#include <SD.h>
#include <SPI.h>
#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"

const int chipSelect = 10;
AudioInfo info(44100, 2, 16);
I2SStream i2s_1;  // final output port 0
I2SStream i2s_2;  // final output port 1
ChannelsSelectOutput out;
WAVDecoder wav;
EncodedAudioOutput decoder(&out, &wav);  // Decoding stream
StreamCopy copier;
File audioFile;

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup file
  SD.begin(chipSelect);
  audioFile = SD.open("/Music/ch4.wav");

  // setup i2s
  auto config1 = i2s_1.defaultConfig(TX_MODE);
  config1.copyFrom(info);
  config1.port_no = 0;
  i2s_1.begin(config1);
  auto config2 = i2s_2.defaultConfig(TX_MODE);
  config2.copyFrom(info);
  config2.port_no = 1;
  i2s_2.begin(config2);

  // split channels to different i2s ports
  out.addOutput(i2s_1, 0, 1);
  out.addOutput(i2s_2, 2, 3);
  // you might want to update the format changes automatically
  //out.addNotifyAudioChange(i2s_1); 
  //out.addNotifyAudioChange(i2s_2);
  out.begin(info);

  // setup decoder
  decoder.begin();

  // begin copy
  copier.begin(decoder, audioFile);
}

void loop() {
  if (!copier.copy()) {
    stop();
  }
}
