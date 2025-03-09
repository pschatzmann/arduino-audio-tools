
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "AudioTools/Disk/SDIndex.h"

void setup() {
  Serial.begin(115200);
  while(!Serial);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);  
  while (!SD.begin(PIN_AUDIO_KIT_SD_CARD_CS)) {
       Serial.println("SD.begin failed");
       delay(1000);
  }
  SDIndex<fs::SDFS,fs::File> idx{SD};
  idx.ls(Serial, "/", "mp3","*");

}

void loop() {
  delay(1000);
}