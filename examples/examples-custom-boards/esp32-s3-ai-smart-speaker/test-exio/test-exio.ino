/**
 * @file test-extio.ino
 * @brief Testing gpio inputs for ESP32-S3-AI-Smart-Speaker: you can call
 * digitalRead() and digitalWrite() on all pins including the EXIO pins.
 * The EXIO are mapped to starting at 1000: EXIO10 = 1010, EXIO11 = 1011, EXIO12 = 1012, ...
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "ESP32S3AISmartSpeaker.h"

AudioInfo info(44100, 2, 16);
AudioBoardStream board(ESP32S3AISmartSpeaker);  // Access I2S as stream
CsvOutput<int16_t> csvOutput(Serial);
StreamCopy copier(csvOutput, board);  // copy board to csvOutput

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // setup board with default config
  board.begin();
}

// Arduino loop - print keys
void loop() {
  int key1 = board.digitalRead(EXIO10);
  int key2 = board.digitalRead(EXIO11);
  int key3 = board.digitalRead(EXIO12);

  char msg[100];
  snprintf(msg, 100, "Keys: %d %d %d", key1, key2, key3);
  Serial.println(msg);

  delay(1000);
}
