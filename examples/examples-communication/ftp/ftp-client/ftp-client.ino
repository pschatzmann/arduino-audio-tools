/**
 * @file ftp-client.ino
 * @author Phil Schatzmann
 * @brief Receiving audio via FTP and writing to I2S of the AudioKit.
 * Replace the userids, passwords and ip adresses with your own!
 * And don't forget to read the Wiki of the imported projects
 * @version 0.1
 * @date 2023-11-09
 *
 * @copyright Copyright (c) 2023
 */

#include "WiFi.h"
#include "ArduinoFTPClient.h" // install https://github.com/pschatzmann/TinyFTPClient
#include "AudioTools.h" // https://github.com/pschatzmann/arduino-audio-tools
#include "AudioCodecs/CodecMP3Helix.h" // https://github.com/pschatzmann/arduino-libhelix
#include "AudioLibs/AudioKit.h" // https://github.com/pschatzmann/arduino-audiokit

WiFiClient cmd;
WiFiClient data;
FTPClient client(cmd, data);
FTPFile file;
AudioKitStream kit; // or replace with e.g. I2SStream
MP3DecoderHelix mp3;
EncodedAudioStream decoder(&kit, &mp3); 
StreamCopy copier(decoder, file);

void setup() {
  Serial.begin(115200);

  // connect to WIFI
  WiFi.begin("network name", "password");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // optional logging
  FTPLogger::setOutput(Serial);
  // FTPLogger::setLogLevel(LOG_DEBUG);

  // open connection
  client.begin(IPAddress(192, 168, 1, 10), "user", "password");

  // copy data from file
  file = client.open("/data/music/Neil Young/After the Gold Rush/08 Birds.mp3");

  // open output device
  kit.begin();
  decoder.begin();

}

void loop() { copier.copy(); }