/**
 * @brief Tesing the MP3DecoderHelix: Decoding
 * on the input side.
 */
// install https://github.com/pschatzmann/arduino-libhelix.git

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "BabyElephantWalk60_mp3.h"

MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
//CsvOutput out(Serial);  
AudioBoardStream out(AudioKitEs8388V1);
EncodedAudioStream pcm_source(&mp3, new MP3DecoderHelix()); // output to decoder
StreamCopy copier(out, pcm_source);   

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  
 
  // setup I2s
  out.begin(out.defaultConfig());

  // make sure that i2s is updated
  pcm_source.addNotifyAudioChange(out);
  // setup pcm_source
  pcm_source.begin();
}

void loop(){
  copier.copy();
}