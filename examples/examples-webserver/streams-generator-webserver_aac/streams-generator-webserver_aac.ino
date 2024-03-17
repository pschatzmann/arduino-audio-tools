/**
 * @file streams-generator-server_aac.ino
 *
 *  This sketch generates a test sine wave. The result is provided as AAC stream which can be listened to in a Web Browser
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#include "AudioTools.h"
#include "AudioCodecs/CodecAACFDK.h"


// WIFI
const char *ssid = "ssid";
const char *password = "password";

AudioInfo info(16000,1,16);
AACEncoderFDK fdk;
AudioEncoderServer server(&fdk, ssid, password);
SineWaveGenerator<int16_t> sineWave;            // Subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in(sineWave);     // Stream generated from sine wave

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Info);

  // configure FDK to use less RAM (not necessary if you activate PSRAM)
  fdk.setAudioObjectType(2);  // AAC low complexity
  fdk.setOutputBufferSize(1024); // decrease output buffer size
  fdk.setVariableBitrateMode(2); // low variable bitrate

  // start server
  server.begin(in, info);

  // start generation of sound
  sineWave.begin(info, N_B4);
}


// copy the data
void loop() {
  server.copy();
}