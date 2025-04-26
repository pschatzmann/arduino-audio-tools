
/**
 * @file send-mp3.ino
 * @brief Example of sending an mp3 stream over Serial the AudioTools library.
 * We use xon/xoff to control the flow of the data.
 */

#include "AudioTools.h"

URLStream url("ssid", "password");  // or replace with ICYStream to get metadata
StreamCopy copier(Serial1, url);         // copy url to decoder
// xon/xoff flow control
const char xon = 17;
const char xoff = 19;
bool is_active = false;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Error);

  // setup serial data sink
  Serial1.begin(115200);

  // mp3 radio
  url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128", "audio/mp3");
}

// Determine if we can send data from the flow control sent by the receiver
bool isAcive() {
  char c = Serial1.read();
  switch (c) {
    case xon:
      is_active = true;
      break;
    case xoff:
      is_active = false;
      break;
  }
  return is_active;
}

void loop() {
  if (isActive()) {
    copier.copy();
  }
}