/**
 * @file player-sdfat-a2dp.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-player/player-sdfat-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

//remove this, to find issues regarding mp3 decoding
#define HELIX_LOGGING_ACTIVE false

#include "AudioTools.h"
#include "AudioTools/AudioLibs/A2DPStream.h"
#include "AudioTools/Disk/AudioSourceSDFAT.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSDFAT source(startFilePath, ext); // , PIN_AUDIO_KIT_SD_CARD_CS);
A2DPStream out;
MP3DecoderHelix decoder;
AudioPlayer player(source, out, decoder);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // setup player
  // Setting up SPI if necessary with the right SD pins by calling 
  // SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);
  player.setVolume(0.1);
  player.begin();

  // setup output - We send the test signal via A2DP - so we conect to a Bluetooth Speaker
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.silence_on_nodata = true; // prevent disconnect when there is no audio data
  cfg.name = "LEXON MINO L";  // set the device here. Otherwise the first available device is used for output
  //cfg.auto_reconnect = true;  // if this is use we just quickly connect to the last device ignoring cfg.name
  out.begin(cfg);


}

void loop() {
  player.copy();
}
