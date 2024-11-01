/**
 * @file stream-memory_mp3-analog.ino
 * @author Phil Schatzmann
 * @brief decode MP3 stream and output as analog signal
 * @version 0.1
 * @date 2021-01-24
 * 
 * @copyright Copyright (c) 2021
 
 */


#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "BabyElephantWalk60_mp3.h"


MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
PWMAudioOutput out;  // PWM output 
EncodedAudioStream decoded(&out, new MP3DecoderHelix()); // output to decoder
StreamCopy copier(decoded, mp3);    // copy in to out

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  // begin processing
  auto cfg = out.defaultConfig();
  out.begin(cfg);

  decoded.begin();
}

void loop(){
  if (mp3) {
    copier.copy();
  } else {
    auto info = decoded.decoder().audioInfo();
    LOGI("The audio rate from the mp3 file is %d", info.sample_rate);
    LOGI("The channels from the mp3 file is %d", info.channels);
    out.end();
    stop();
  }
}