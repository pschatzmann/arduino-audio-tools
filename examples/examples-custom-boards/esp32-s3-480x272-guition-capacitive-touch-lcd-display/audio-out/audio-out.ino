/**
 * @file audio-out.ino
 * @brief Sine wave playback for the Guition ESP32-S3 4.3" 480x272 display's
 * onboard NS4168 I2S speaker amp (no codec/mic on this board - unlike the
 * ES8311-based esp32-s3-240x320-lcd-display example this was ported from,
 * so this uses plain I2SStream rather than AudioBoardStream).
 *
 * IMPORTANT: kPinBck/kPinWs/kPinData below are CONFIRMED WORKING on real
 * hardware (user-supplied, 2026-08-19). This supersedes two earlier
 * guesses that turned out wrong: the original pin-availability-
 * elimination guess (BCK=18/WS=17/DATA=38, no sound), and a first
 * "confirmed" pin set (WS=6/BCK=7/DATA=15) that was itself superseded by
 * this one. Note these confirmed pins - BCK=42, WS=2, DATA=41 - collide
 * with lcd-test.ino's still-guessed RGB sync/backlight pins (PCLK=42,
 * Backlight=2, VSYNC=41), which means that guessed RGB table is wrong
 * too (a single GPIO can't serve both). If you're bringing up the
 * display on this same board, expect to have to correct lcd-test.ino's
 * pin table as well.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"

// Confirmed working on real hardware - see file header.
constexpr int kPinBck = 42;
constexpr int kPinWs = 2;
constexpr int kPinData = 41;

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave;
GeneratedSoundStream<int16_t> sound(sineWave);
I2SStream out;
StreamCopy copier(out, sound);  // copies sound into i2s

void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  if (kPinBck < 0 || kPinWs < 0 || kPinData < 0) {
    Serial.println(
        "error: set kPinBck/kPinWs/kPinData to this board's actual I2S "
        "pins before flashing (see file header)");
    stop();
  }

  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.pin_bck = kPinBck;
  config.pin_ws = kPinWs;
  config.pin_data = kPinData;
  if (!out.begin(config)) {
    Serial.println("error!");
    stop();
  }

  sineWave.begin(info, N_B4);
  sineWave.setAmplitude(4000);  // plain I2SStream has no setVolume(); scale here instead
  Serial.println("started...");
}

void loop() { copier.copy(); }
