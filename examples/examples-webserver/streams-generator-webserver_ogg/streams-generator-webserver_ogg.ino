/**
 * @file streams-generator-server_ogg.ino
 *
 * This sketch generates a test sine wave. The result is provided as opus ogg
 * stream which can be listened to in a Web Browser This seems to be quite
 * unreliable in the browser and with ffplay  -i http://address it is breaking
 * up.
 *
 * Only saving it to a file for playback seems to help: ffmpeg  -i
 * http://address test.ogg
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */

#include "AudioTools.h"
#include "AudioCodecs/CodecOpusOgg.h"

// WIFI
const char *ssid = "ssid";
const char *password = "password";

AudioInfo info(16000, 1, 16);
OpusOggEncoder ogg;
AudioEncoderServer server(&ogg, ssid, password);
SineWaveGenerator<int16_t> sineWave;
GeneratedSoundStream<int16_t> in(sineWave);  // Stream generated from sine wave

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start server
  server.begin(in, info);

  // start generation of sound
  sineWave.begin(info, N_B4);
}

// copy the data
void loop() { server.copy(); }