/**
 * @file player-sd-vs1053.ino
 * @brief Audio player which sends the output to a VS1053 module
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// install https://github.com/pschatzmann/arduino-vs1053.git

#include "AudioTools.h"
#include "AudioLibs/VS1053Stream.h"
#include "AudioLibs/AudioSourceSDFAT.h"
#include "AudioCodecs/CodecCopy.h"

#define SD_CARD_CS  13

const char *startFilePath="/";
const char* ext="mp3";
SdSpiConfig sdcfg(SD_CARD_CS, DEDICATED_SPI, SD_SCK_MHZ(10) , &SPI);
AudioSourceSDFAT source(startFilePath, ext, sdcfg);
VS1053Stream vs1053; // final output
AudioPlayer player(source, vs1053, *new CopyDecoder());


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  SPI.begin(SD_CARD_CS);

  // setup output
  auto cfg = vs1053.defaultConfig();
  cfg.is_encoded_data = true; // vs1053 is accepting encoded data
  // Use your custom pins or define in AudioCodnfig.h
  //cfg.cs_pin = VS1053_CS; 
  //cfg.dcs_pin = VS1053_DCS;
  //cfg.dreq_pin = VS1053_DREQ;
  //cfg.reset_pin = VS1053_RESET;
  vs1053.begin(cfg);

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
