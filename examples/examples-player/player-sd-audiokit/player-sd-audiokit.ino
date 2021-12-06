/**
 * @file player-sd-audiokit.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-player/player-sd-audiokit/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// set this in AudioConfig.h or here after installing https://github.com/pschatzmann/arduino-libhelix.git
#define USE_SDFAT
#define USE_HELIX
#include "AudioTools.h"
#include "AudioDevices/ESP32AudioKit/AudioKit.h"

using namespace audio_tools;  

const char *startFilePath="/";
const char* ext="mp3";
int speedMz = 10;
AudioSourceSdFat source(startFilePath, ext, PIN_AUDIO_KIT_SD_CARD_CS, speedMz);
AudioKitStream i2s;
MP3DecoderHelix decoder;
AudioPlayer player(source, i2s, decoder);


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup output
  auto cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);
  
  // setup player
  player.begin();
}

void loop() {
  player.copy();
}
