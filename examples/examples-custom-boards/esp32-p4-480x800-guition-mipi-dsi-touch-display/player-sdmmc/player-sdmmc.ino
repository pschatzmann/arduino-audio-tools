/**
 * @file player-sdmmc.ino
 * @brief MP3 player for the Guition JC4880P443C_I_W (ESP32-P4): reads
 * files off the microSD card (SD_MMC, 4-bit) and plays them through the
 * ES8311/NS4150 speaker path via I2SCodecStream.
 * 
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 * - https://github.com/pschatzmann/arduino-libhelix
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <SD_MMC.h>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"
#include "AudioTools/Disk/AudioSourceSDMMC.h"

// SD pins, see sdmmc-test.ino.
constexpr int kPinSdClk = 43;
constexpr int kPinSdCmd = 44;
constexpr int kPinSdD0 = 39;
constexpr int kPinSdD1 = 40;
constexpr int kPinSdD2 = 41;
constexpr int kPinSdD3 = 42;
constexpr int kSdLdoChannel = 4;

// Audio pins, see audio-out.ino.
constexpr int kPinI2sMclk = 13;
constexpr int kPinI2sBck = 12;
constexpr int kPinI2sWs = 10;
constexpr int kPinI2sDout = 9;
constexpr int kPinCodecScl = 8;
constexpr int kPinCodecSda = 7;
constexpr int kPinAmpEnable = 11;  // unverified polarity - see audio-out.ino

const char *startFilePath = "/";
const char *ext = "mp3";
AudioSourceSDMMC source(startFilePath, ext);
I2SCodecStream out(GenericES8311);
MP3DecoderHelix decoder;
AudioPlayer player(source, out, decoder);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  pinMode(kPinAmpEnable, OUTPUT);
  digitalWrite(kPinAmpEnable, HIGH);  // enable NS4150 - see audio-out.ino

  Wire.begin(kPinCodecSda, kPinCodecScl);

#ifdef SOC_SDMMC_IO_POWER_EXTERNAL
  SD_MMC.setPowerChannel(kSdLdoChannel);
#endif
  if (!SD_MMC.setPins(kPinSdClk, kPinSdCmd, kPinSdD0, kPinSdD1, kPinSdD2,
                      kPinSdD3)) {
    Serial.println("SD_MMC.setPins() failed");
    stop();
  }

  auto cfg = out.defaultConfig(TX_MODE);
  cfg.pin_mck = kPinI2sMclk;
  cfg.pin_bck = kPinI2sBck;
  cfg.pin_ws = kPinI2sWs;
  cfg.pin_data = kPinI2sDout;
  out.begin(cfg);
  out.setVolume(0.5);

  // AudioSourceSDMMC::begin() calls SD_MMC.begin("/sdcard", true) - forces
  // 1-bit mode regardless of setPins() above (D1-D3 simply go unused);
  // fine for MP3 playback, just not using this board's full 4-bit wiring.
  player.begin();
}

void loop() { player.copy(); }
