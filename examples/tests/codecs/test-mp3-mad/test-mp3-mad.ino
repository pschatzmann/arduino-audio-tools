/**
 * @brief Testing MP3DecoderMAD
 * 
 */
// install https://github.com/pschatzmann/arduino-libhelix.git

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioCodecs/CodecMP3MAD.h"
#include "BabyElephantWalk60_mp3.h"

MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
//CsvOutput<int16_t> out(Serial,2);  
AudioKitStream out;
EncodedAudioStream decoder(&out, new MP3DecoderMAD()); // output to decoder
StreamCopy copier(decoder, mp3);   

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // setup I2s
  out.begin(out.defaultConfig());

  decoder.begin();
}

void loop(){
  copier.copy();
}