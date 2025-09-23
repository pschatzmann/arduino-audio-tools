
/**
 * @file send-mp3.ino
 * @brief ESP32 Example of sending an mp3 stream over Serial the AudioTools library.
 * The processing must be synchronized with RTS/CTS flow control to prevent a
 * buffer overflow and lost data. We get the mp3 from an URLStream.
 * We used a ESP32 Dev Module for the test.
 */
#include <HardwareSerial.h>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/Communication/AudioHttp.h"

URLStream url("ssid", "password");  // or replace with ICYStream to get metadata
HardwareSerial MP3Serial(1);             // define a Serial for UART1
StreamCopy copier(MP3Serial, url);       // copy url to decoder
// pins
const int MySerialTX = 17; // TX2
const int MySerialRX = -1; // not used
const int MySerialRTS = 4; // GPIO 4
const int MySerialCTS = -1; // not useed

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // improve performance
  MP3Serial.setTxBufferSize(1024); // must be called before begin

  // setup serial data sink with flow control
  MP3Serial.begin(115200, SERIAL_8N1);
  MP3Serial.setPins(MySerialRX, MySerialTX, MySerialCTS, MySerialRTS);
  MP3Serial.setHwFlowCtrlMode(UART_HW_FLOWCTRL_CTS_RTS);

  // mp3 radio
  url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128", "audio/mp3");
}

void loop() { copier.copy(); }