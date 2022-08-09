/**
 * @file player-sd-vs1053.ino
 * @brief Audio player which sends the output to a VS1053 module
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// install https://github.com/baldram/ESP_VS1053_Library.git

#include "AudioTools.h"
#include "AudioLibs/VS1053Stream.h"
#include "AudioLibs/AudioSourceSdFat.h"
#include "AudioCodecs/CodecCopy.h"

#define VS1053_CS     5
#define VS1053_DCS    16
#define VS1053_DREQ   4
#define SD_CARD_CS    13
#define SD_CARD_MISO 2
#define SD_CARD_MOSI 15
#define SD_CARD_CLK  14

const char *startFilePath="/";
const char* ext="mp3";
SdSpiConfig sdcfg(PIN_AUDIO_KIT_SD_CARD_CS, DEDICATED_SPI, SD_SCK_MHZ(10) , &SPI);
AudioSourceSdFat source(startFilePath, ext, sdcfg);
VS1053Stream vs1053(VS1053_CS,VS1053_DCS, VS1053_DREQ); // final output
AudioPlayer player(source, vs1053, *new CodecCopy());


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  SPI.begin(SD_CARD_CLK, SD_CARD_MISO, SD_CARD_MOSI, SD_CARD_CS);

  // setup output
  vs1053.begin();

  // setup player
  player.setVolume(0.7);
  player.begin();

  // select file with setPath() or setIndex()
  //player.setPath("/ZZ Top/Unknown Album/Lowrider.mp3");
  //player.setIndex(1); // 2nd file

}

void loop() {
  player.copy();
}
