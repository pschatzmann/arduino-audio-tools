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

// additional controls
const int volumePin = A0;
Debouncer nextBuffonDebouncer(2000);
const int nextButtonPin = 13; // Sensitive touch 4 
const int buttonPressedLimit = 30;// touch limit -> increase to make more sensitive

void printMetaData(MetaDataType type, const char* str, int len){
  Serial.print("==> ");
  Serial.print(MetaDataTypeStr[type]);
  Serial.print(": ");
  Serial.println(str);
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup output
  auto cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);

  // setup player
  player.setCallbackMetadata(printMetaData);
  player.begin();
}

// Sets the volume control from a linear potentiometer input
void updateVolume() {
  // Reading potentiometer value (range is 0 - 4095)
  float vol = static_cast<float>(analogRead(volumePin));
  // min in 0 - max is 1.0
  player.setVolume(vol/4095.0);
}


// Moves to the next url when we touch the pin
void updatePosition() {
   if (touchRead(nextButtonPin) < buttonPressedLimit) {
      Serial.println("Moving to next url");
      if (debouncer.debounce()){
        player.next();
      }
  }
}


void loop() {
  //updateVolume(); // remove comments to activate volume control
  //updatePosition();  // remove comments to activate position control
  player.copy();
}