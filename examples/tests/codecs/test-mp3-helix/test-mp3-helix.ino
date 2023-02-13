/**
 * @brief Testing decoded data from source via readBytes()
 * 
 */
// install https://github.com/pschatzmann/arduino-libhelix.git

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "BabyElephantWalk60_mp3.h"

MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
CsvStream<int16_t> csv(Serial,2);  
EncodedAudioStream decoded(&mp3, new MP3DecoderHelix()); // output to decoder
StreamCopy copier(csv, decoded);   

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  
  decoded.begin();
}

void loop(){
  copier.copy();
}