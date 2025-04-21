
/**
 * @file send-mp3.ino
 * @brief Example of sending an mp3 stream over Serial the AudioTools library.
 * We use a custom pin to control the flow of the data. If we are ready to
 * receive data, we set the pin to LOW. 
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

URLStream url("ssid", "password");  // or replace with ICYStream to get metadata
AudioBoardStream i2s(AudioKitEs8388V1);  // final output of decoded stream
StreamCopy copier(Serial1, url);       // copy url to decoder
// pins
const int flowControlPin = 17;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Error);

  // setup flow control pin
  pinMode(flowControlPin, INPUT_PULLUP);  // flow control pin

  // setup serial data sink 
  Serial1.begin(115200);

  // mp3 radio
  url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128", "audio/mp3");
}

void loop() {
  if (digitalRead(flowControlPin) == LOW) {
    copier.copy();
  }
}