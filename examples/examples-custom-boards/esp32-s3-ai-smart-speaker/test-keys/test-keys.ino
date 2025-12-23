/**
 * @file test-keys.ino
 * @brief Testing key inputs for ESP32-S3-AI-Smart-Speaker.
 * The EXIO pins are mapped to starting at 1000: EXIO10 = 1010, EXIO11 = 1011,
 * EXIO12 = 1012, ...
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

AudioBoardStream board(ESP32S3AISmartSpeaker);  // Access I2S as stream

// event handler for key presses
void handleKey(bool active, int gpio, void*) {
  Serial.print("Key event ");
  Serial.print(gpio);
  Serial.print(" -> ");
  Serial.println(active);
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);
  delay(3000);

  // setup actions for keys
  board.addAction(board.getKey(1), handleKey);
  board.addAction(board.getKey(2), handleKey);
  board.addAction(board.getKey(3), handleKey);

  // setup board with default config
  board.begin();
}

// Arduino loop - copy data
void loop() { board.processActions(); }
