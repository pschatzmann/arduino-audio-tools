/**
 * @file streams-generator-server_wav.ino
 *
 *  This sketch generates a test sine wave. The result is provided as WAV stream which can be listened to in a Web Browser
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioServerEx.h"

// WIFI
const char *ssid = "SSID";
const char *password = "password";

AudioInfo info(10000, 1, 16);
SineWaveGenerator<int16_t> sineWave;            // Subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in(sineWave);     // Stream generated from sine wave
AudioWAVServerEx server;


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Info);
  HttpLogger.setLevel(tinyhttp::Info);

  // start server
  auto cfg = server.defaultConfig();
  cfg.copyFrom(info);
  cfg.ssid = ssid;
  cfg.password = password; 
  cfg.input = &in; // Define input
  server.begin(cfg);

  // start generation of sound
  sineWave.begin(info, N_B4);
  in.begin();
}


// copy the data
void loop() {
  server.copy();
}
