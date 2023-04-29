/**
 * @brief Tesing the MP3DecoderHelix
 * 
 */
// install https://github.com/pschatzmann/arduino-libhelix.git

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "BabyElephantWalk60_mp3.h"

MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
//CsvOutput<int16_t> out(Serial,2);  
AudioKitStream out;
EncodedAudioStream decoder(&out, new MP3DecoderHelix()); // output to decoder
StreamCopy copier(decoder, mp3);   

void setup(){
  Serial.begin(115200);
 
  // setup I2s
  out.begin(out.defaultConfig());

  AudioLogger::instance().begin(Serial, AudioLogger::Info);  
  decoder.begin();
}

void loop(){
  copier.copy();
}