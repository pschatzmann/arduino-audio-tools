#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "WiFi.h"

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
AudioSourceURL source(urlStream, urls, 5, 0);
I2SStream i2s;
MP3DecoderHelix decoder;
AudioPlayer player(source, i2s, decoder);


void printMetaData(MetaDataType type, const char* str, int len){
  Serial.print("==> ");
  Serial.print(MetaDataTypeStr[type]);
  Serial.print(": ");
  Serial.println(str);
}

void setup() {
  // setup output
  I2SConfig cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);

  // setup player
  player.setCallbackMetadata(printMetaData);
  player.begin();
}

void loop() {
  player.copy();
}