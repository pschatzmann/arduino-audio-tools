
/**
 * @file send-mp3.ino
 * @brief Example of sending an mp3 stream over Serial the AudioTools library.
 * We use xon/xoff to control the flow of the data.
 * I am using an ESP32 Dev Module for the test with the pins TX=17 and RX=16.
 */

#include "AudioTools.h"
#include "AudioTools/Communication/AudioHttp.h"

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
  Serial2.begin(115200);

  // mp3 radio
  url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128", "audio/mp3");
}

// Determine if we can send data from the flow control sent by the receiver
bool isActive() {
  char c = Serial2.read();
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