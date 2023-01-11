/**
 * @file streams-effect-server_wav.ino
 *
 *  This sketch uses sound effects applied to a sine wav. The result is provided as WAV stream which can be listened to in a Web Browser
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#include "AudioTools.h"

// WIFI
const char *ssid = "ssid";
const char *password = "password";
AudioWAVServer server(ssid, password);

// Contorl input
float volumeControl = 1.0;
int16_t clipThreashold = 4990;
float fuzzEffectValue = 6.5;
int16_t distortionControl = 4990;
int16_t tremoloDuration = 200;
float tremoloDepth = 0.5;

// Audio 
SineWaveGenerator<int16_t> sine;
GeneratedSoundStream<int16_t> stream(sine); 
AudioEffectStream effects(stream);

// Audio Format
const int sample_rate = 10000;
const int channels = 1;


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Info);

  // setup effects
  effects.addEffect(new Boost(volumeControl));
  effects.addEffect(new Distortion(clipThreashold));
  effects.addEffect(new Fuzz(fuzzEffectValue));
  effects.addEffect(new Tremolo(tremoloDuration, tremoloDepth, sample_rate));

  // start server
  auto config = stream.defaultConfig();
  config.channels = channels;
  config.sample_rate = sample_rate;
  server.begin(effects, config);
  sine.begin(config, N_B4);
  stream.begin(config);
  effects.begin(config);

}

// copy the data
void loop() {
  server.copy();
}