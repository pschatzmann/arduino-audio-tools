
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#define USE_SDFAT
#include <SPI.h>
#include <SdFat.h>
#include "AudioTools/Disk/SDIndex.h"
#define SD_FAT_TYPE 3

#if SD_FAT_TYPE == 0
  typedef SdFat AudioFs;
  typedef File AudioFile;
#elif SD_FAT_TYPE == 1
  typedef SdFat32 AudioFs;
  typedef File32 AudioFile;
#elif SD_FAT_TYPE == 2
  typedef SdExFat AudioFs;
  typedef ExFile AudioFile;
#elif SD_FAT_TYPE == 3
  typedef SdFs AudioFs;
  typedef FsFile AudioFile;
#else  // SD_FAT_TYPE
#endif


AudioFs sd;

void setup() {
  Serial.begin(115200);
  while(!Serial);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  SdSpiConfig cfg(PIN_AUDIO_KIT_SD_CARD_CS, DEDICATED_SPI, SD_SCK_MHZ(2));
  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);  
  while (!sd.begin(cfg)) {
       Serial.println("sd.begin failed");
       delay(1000);
  }
  SDIndex<AudioFs,AudioFile> idx(sd);
  idx.ls(Serial, "/", "mp3","*");

}

void loop() {
  delay(1000);
}