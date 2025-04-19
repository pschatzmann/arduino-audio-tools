
/**
 * @file send-mp3.ino
 * @brief Example of sending an mp3 stream over Serial the AudioTools library.
 * The processing must be synchronized with RTS/CTS flow control to prevent a
 * buffer overflow and lost data.
 * We get the mp3 from an URLStream.
 */
#include <HardwareSerial.h>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

URLStream url("ssid", "password");  // or replace with ICYStream to get metadata
AudioBoardStream i2s(AudioKitEs8388V1);  // final output of decoded stream
HardwareSerial MP3Serial(1);             // define a Serial for UART1
StreamCopy copier(MP3Serial, url);       // copy url to decoder
// pins
const int MySerialTX = 17;
const int MySerialRX = -1;
const int MySerialRTS = 19;
const int MySerialCTS = -1;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup serial data sink with flow control
  MP3Serial.begin(115200, SERIAL_8N1);
  MP3Serial.setPins(MySerialRX, MySerialTX, MySerialCTS, MySerialRTS);
  MP3Serial.setHwFlowCtrlMode(HW_FLOWCTRL_CTS_RTS);

  // mp3 radio
  url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128", "audio/mp3");
}

void loop() { copier.copy(); }