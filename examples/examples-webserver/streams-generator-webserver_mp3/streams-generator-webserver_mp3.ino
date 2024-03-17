/**
 * @file streams-generator-server_ogg.ino
 *
 *  This sketch generates a test sine wave. The result is provided as mp3 stream which can be listened to in a Web Browser
 *  Please note that MP3EncoderLAME needs a processor with PSRAM !
 *  
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3LAME.h"

// WIFI
const char *ssid = "ssid";
const char *password = "password";

AudioInfo info(24000, 1, 16);
MP3EncoderLAME mp3;
AudioEncoderServer server(&mp3, ssid, password);
SineWaveGenerator<int16_t> sineWave;            // Subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in(sineWave);     // Stream generated from sine wave

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Info);

  // start server
  server.begin(in, info);

  // start generation of sound
  sineWave.begin(info, N_B4);
}


// copy the data
void loop() {
  server.copy();
}