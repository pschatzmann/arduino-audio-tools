#include "AudioTools.h"

using namespace audio_tools;

// WIFI
const char *ssid = "ssid";
const char *password = "password";

AudioWAVServer server(ssid, password);

// Sound Generation
const int sample_rate = 10000;
const int channels = 1;

SineWaveGenerator<int16_t> sineWave;                      // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in(sineWave, channels);     // Stream generated from sine wave


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Debug);

  // start server
  server.begin(in, sample_rate, channels);

  // start generation of sound
  sineWave.begin(sample_rate, B4);
  in.begin();
}


// copy the data
void loop() {
  server.doLoop();
}