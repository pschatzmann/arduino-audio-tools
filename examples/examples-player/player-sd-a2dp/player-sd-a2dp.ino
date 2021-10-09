#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"

using namespace audio_tools;  

const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSdFat source(startFilePath, ext);
A2DPStream out = A2DPStream::instance() ;                 // A2DP input - A2DPStream is a singleton!
MP3DecoderHelix decoder;
AudioPlayer player(source, out, decoder);


void printMetaData(MetaDataType type, const char* str, int len){
  Serial.print("==> ");
  Serial.print(MetaDataTypeStr[type]);
  Serial.print(": ");
  Serial.println(str);
}

void setup() {
  // setup output - We send the test signal via A2DP - so we conect to the MyMusic Bluetooth Speaker
  out.begin(TX_MODE, "MyMusic");

  // setup player
  source.setFileFilter("*Bob Dylan*");
  player.setCallbackMetadata(printMetaData);
  player.begin();
}

void loop() {
  player.copy();
}