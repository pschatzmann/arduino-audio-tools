/**
 * @file player-sd-vs1053.ino
 * @brief Audio player which sends the output to a VS1053 module. Using a module with built in SD card
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// install https://github.com/pschatzmann/arduino-vs1053.git

#include "AudioTools.h"
#include "AudioLibs/VS1053Stream.h"
#include "AudioLibs/AudioSourceSD.h"
#include "AudioCodecs/CodecCopy.h"

const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSD source(startFilePath, ext);
VS1053Stream vs1053; // final output
AudioPlayer player(source, vs1053, *new CopyDecoder());


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup output
  auto cfg = vs1053.defaultConfig();
  cfg.is_encoded_data = true; // vs1053 is accepting encoded data
  // Use your custom pins or define in AudioCodnfig.h
  //cfg.cs_pin = VS1053_CS; 
  //cfg.dcs_pin = VS1053_DCS;
  //cfg.dreq_pin = VS1053_DREQ;
  //cfg.reset_pin = VS1053_RESET;
  vs1053.begin(cfg);
  vs1053.setVolume(1.0); // full volume 

  // setup player
  player.begin();

  // select file with setPath() or setIndex()
  //player.setPath("/ZZ Top/Unknown Album/Lowrider.mp3");
  //player.setIndex(1); // 2nd file

}

void loop() {
  player.copy();
}
