/**
 * @file player-sdfat-a2dp.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-player/player-sdfat-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "AudioTools.h"
#include "AudioLibs/AudioA2DP.h"
#include "AudioLibs/AudioSourceSDFAT.h"
#include "AudioLibs/AudioKit.h"
#include "AudioCodecs/CodecMP3Helix.h"

const char *startFilePath="/";
const char* ext="mp3";
SdSpiConfig sdcfg(PIN_AUDIO_KIT_SD_CARD_CS, DEDICATED_SPI, SD_SCK_MHZ(10) , &AUDIOKIT_SD_SPI);
AudioSourceSDFAT source(startFilePath, ext, sdcfg);
A2DPStream out;    // A2DP input - A2DPStream is a singleton!
MP3DecoderHelix decoder;
AudioPlayer player(source, out, decoder);


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Define SPI FOR SD card
  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);

  // setup output - We send the test signal via A2DP - so we conect to the "LEXON MINO L" Bluetooth Speaker
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.name = "LEXON MINO L";
  //cfg.auto_reconnect = true;  // if this is use we just quickly connect to the last device ignoring cfg.name
  out.begin(cfg);

  // setup player
  player.setVolume(0.1);
  player.begin();

}

void loop() {
  player.copy();
}