#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"

using namespace audio_tools;  

const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSdFat source(startFilePath, ext);
AnalogAudioStream out;
MP3DecoderHelix decoder;
AudioPlayer player(source, out, decoder);


void printMetaData(MetaDataType type, const char* str, int len){
  Serial.print("==> ");
  Serial.print(MetaDataTypeStr[type]);
  Serial.print(": ");
  Serial.println(str);
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Debug);

  // setup output
  auto cfg = out.defaultConfig();
  out.begin(cfg);

  // setup player
  //source.setFileFilter("*Bob Dylan*");
  player.setCallbackMetadata(printMetaData);
  player.begin();
}

void loop() {
  player.copy();
}
