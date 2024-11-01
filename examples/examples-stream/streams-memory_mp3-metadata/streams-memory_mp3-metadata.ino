/**
 * @file streams-memory_mp3-metadata.ino
 * @brief In this example we provide the metadata from a MP3 file. 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "sample-12s.h"



MemoryStream mp3(sample_12s_mp3, sample_12s_mp3_len);
MetaDataOutput out;
StreamCopy copier(out, mp3); // copy in to out

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

  mp3.begin();

  out.setCallback(printMetaData);
  out.begin();
}

void loop(){
  if (mp3) {
    copier.copy();
  } else {
    stop();
  }
}
