/**
 * @file communication-generator-rtsp.ino
 * @author Phil Schatzmann
 * @brief Provide generated sine tone via RTSP. Depends on https://github.com/pschatzmann/Micro-RTSP-Audio
 * @version 0.1
 * @date 2022-05-02
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/RTSP.h"
#include "AudioStreamer.h"
#include "RTSPServer.h"

int port = 554;
int channels = 1;
int sample_rate = 16000;
int bits_per_sample = 16;
const char* wifi = "ssid";
const char* password = "password";

SineFromTable<int16_t> sineWave(32000);           // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);    // Stream generated from sine wave
RTSPSourceFromAudioStream source(sound);              // Stream sound via RTSP
AudioStreamer streamer = AudioStreamer(&source);
RTSPServer rtsp = RTSPServer(&streamer, port);


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // Setup sine wave
  auto cfgS = sineWave.defaultConfig();
  cfgS.channels = channels;
  cfgS.sample_rate = sample_rate;
  cfgS.bits_per_sample = bits_per_sample;
  sineWave.begin(cfgS, N_B4);

  // Start Wifi & rtsp server
  rtsp.begin(wifi, password);
}

void loop() { delay(1000); }