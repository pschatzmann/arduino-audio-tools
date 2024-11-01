/**
 * @file streams-sd_mp3-metadata.ino
 * @author Phil Schatzmann
 * @brief read MP3 stream from a SD drive and output metadata and audio! 
 * The used mp3 file contains ID3 Metadata!
 * @date 2021-11-07
 * 
 * @copyright Copyright (c) 2021
 */

// install https://github.com/pschatzmann/arduino-libhelix.git
// install https://github.com/greiman/SdFat.git

#include <SPI.h>
#include <SdFat.h>
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"


//                            -> EncodedAudioStream -> I2SStream
// URLStream -> MultiOutput -|
//                            -> MetaDataOutput

File audioFile;
SdFs SD;
MetaDataOutput outMeta; // final output of metadata
I2SStream i2s; // I2S output
EncodedAudioStream out2dec(&i2s, new MP3DecoderHelix()); // Decoding stream
MultiOutput out;
StreamCopy copier(out, audioFile); // copy url to decoder

// callback for meta data
void printMetaData(MetaDataType type, const char* str, int len){
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");
  Serial.println(str);
}

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  // setup multi output
  out.add(outMeta);
  out.add(out2dec);

  // setup file
  SD.begin(SdSpiConfig(PIN_CS, DEDICATED_SPI, SD_SCK_MHZ(2)));
  //audioFile = SD.open("/Music/Conquistadores.mp3");
  audioFile = SD.open("/test/002.mp3");

  // setup metadata
  outMeta.setCallback(printMetaData);
  outMeta.begin();

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  i2s.begin(config);

  // setup I2S based on sampling rate provided by decoder
  out2dec.begin();

}

void loop(){
  copier.copy();
}
