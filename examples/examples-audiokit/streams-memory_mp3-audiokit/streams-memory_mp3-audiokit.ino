/**
 * @file streams-memory_mp3_short-i2s.ino
 * @author Phil Schatzmann
 * @brief decode MP3 stream and output as i2s signal
 * @version 0.1
 * @date 2021-01-24
 * 
 * @copyright Copyright (c) 2021
 
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "zero.h"

MemoryStream mp3(zero_mp3, zero_mp3_len);
AudioBoardStream i2s(AudioKitEs8388V1); 
MP3DecoderHelix helix;
EncodedAudioStream out(&i2s, &helix); // output to decoder
StreamCopy copier(out, mp3);    // copy in to i2s

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  // begin processing
  auto cfg = i2s.defaultConfig();
  cfg.sample_rate = 24000;
  cfg.channels = 1;
  i2s.begin(cfg);
  out.begin();
}

void loop(){
  if (mp3.available()) {
    copier.copy();
  } else {
    helix.end(); // flush output
    auto info = out.decoder().audioInfo();
    LOGI("The audio rate from the mp3 file is %d", info.sample_rate);
    LOGI("The channels from the mp3 file is %d", info.channels);
    i2s.end();
    stop();
  }
}