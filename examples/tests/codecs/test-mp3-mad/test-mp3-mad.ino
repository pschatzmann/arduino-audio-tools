/**
 * @brief Testing MP3DecoderMAD
 * 
 */
// install https://github.com/pschatzmann/arduino-libhelix.git

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioCodecs/CodecMP3MAD.h"
#include "BabyElephantWalk60_mp3.h"

MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
//CsvOutput<int16_t> out(Serial,2);  
AudioBoardStream out(AudioKitEs8388V1);
EncodedAudioStream decoder(&out, new MP3DecoderMAD()); // output to decoder
StreamCopy copier(decoder, mp3);   

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  // setup I2s
  out.begin(out.defaultConfig());

  decoder.begin();
}

void loop(){
  copier.copy();
}