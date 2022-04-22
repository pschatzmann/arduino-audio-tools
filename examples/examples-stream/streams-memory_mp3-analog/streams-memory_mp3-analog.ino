/**
 * @file stream-memory_mp3-analog.ino
 * @author Phil Schatzmann
 * @brief decode MP3 stream and output as analog signal
 * @version 0.1
 * @date 2021-01-24
 * 
 * @copyright Copyright (c) 2021
 
 */

// install https://github.com/pschatzmann/arduino-libhelix.git

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "BabyElephantWalk60_mp3.h"


MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
AnalogAudioStream analog;  // Analog output 
EncodedAudioStream out(&analog, new MP3DecoderHelix()); // output to decoder
StreamCopy copier(out, mp3);    // copy in to analog

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // update audio info with info from decoder
  out.setNotifyAudioChange(analog);

  // begin processing
  auto cfg = analog.defaultConfig();
  analog.begin(cfg);

  out.begin();
}

void loop(){
  if (mp3) {
    copier.copy();
  } else {
    auto info = out.decoder().audioInfo();
    LOGI("The audio rate from the mp3 file is %d", info.sample_rate);
    LOGI("The channels from the mp3 file is %d", info.channels);
    analog.end();
    stop();
  }
}