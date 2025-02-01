/**
 * @file streams-url_raw-serial.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-url_raw-serial/README.md 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "WiFi.h"
#include "AudioTools.h"

AudioInfo info(8000, 2, 16);
URLStream music;                    // Music Stream
CsvOutput printer(Serial);          // ASCII stream 
StreamCopy copier(printer, music);  // copies music into printer

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  
  // connect to WIFI
  WiFi.begin("network-name", "password");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500); 
  }

  // open music stream - it contains 2 channels of int16_t data
  music.begin("https://pschatzmann.github.io/Resources/audio/audio.raw");

  // open output
  printer.begin(info);
}


// Arduino loop - repeated processing 
void loop() {
  copier.copy();
}