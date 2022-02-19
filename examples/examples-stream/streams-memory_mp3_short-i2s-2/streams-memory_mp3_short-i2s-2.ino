/**
 * @file streams-memory_mp3_short-i2s-2.ino
 * @author Phil Schatzmann
 * @brief decode MP3 stream and output as i2s signal
 * @version 0.1
 * @date 2021-01-24
 * 
 * @copyright Copyright (c) 2021
 
 */

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "zero.h"


MemoryStream mp3(zero_mp3, zero_mp3_len);
I2SStream i2s;  // PWM output 
MP3DecoderHelix helix;
EncodedAudioStream out(&i2s, &helix); // output to decoder
StreamCopy copier(out, mp3);    // copy in to out

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // begin processing
  auto cfg = i2s.defaultConfig();
  cfg.sample_rate = 24000;
  cfg.channels = 1;
  i2s.begin(cfg);
  out.begin();

  sayWord();

}

void sayWord() {
  Serial.println("Saying word...");
  mp3.begin(); // restart source 
  helix.begin(); // so that we can repeatedly call this method
  copier.copyAll();
  helix.end(); // flush output
}

void loop(){
}

