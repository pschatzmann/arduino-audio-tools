/**
 * @file player-sd-a2dp.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-player/player-sd-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// set this in AudioConfig.h or here after installing https://github.com/pschatzmann/arduino-libhelix.git
#define USE_HELIX 
#define USE_A2DP
#define USE_SDFAT

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"



const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSdFat source(startFilePath, ext);
A2DPStream out = A2DPStream::instance();  // A2DP input - A2DPStream is a singleton!
MP3DecoderHelix decoder;
AudioPlayer player(source, out, decoder);


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

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