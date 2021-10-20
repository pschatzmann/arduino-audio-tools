// set this in AudioConfig.h or here after installing https://github.com/pschatzmann/arduino-libhelix.git
#define USE_HELIX 

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"

using namespace audio_tools;  

const char *urls[] = {
  "http://centralcharts.ice.infomaniak.ch/centralcharts-128.mp3",
  "http://centraljazz.ice.infomaniak.ch/centraljazz-128.mp3",
  "http://centralrock.ice.infomaniak.ch/centralrock-128.mp3",
  "http://centralcountry.ice.infomaniak.ch/centralcountry-128.mp3",
  "http://centralfunk.ice.infomaniak.ch/centralfunk-128.mp3"
};
const char *wifi = "wifi";
const char *password = "password";

URLStream urlStream(wifi, password);
AudioSourceURL source(urlStream, urls,"audio/mp3", 5, 0);
I2SStream i2s;
MP3DecoderHelix decoder;
AudioPlayer player(source, i2s, decoder);
const int volumePin = 15;


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
  auto cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);

  // setup player
  player.setCallbackMetadata(printMetaData);
  player.begin();
}

void updateVolume() {
  // Reading potentiometer value (range is 0 - 4095)
  float vol = static_cast<float>(analogRead(volumePin));
  // min in 0 - max is 1.0
  player.setVolume(vol/4095.0);
}

void loop() {
  //updateVolume();
  player.copy();
}