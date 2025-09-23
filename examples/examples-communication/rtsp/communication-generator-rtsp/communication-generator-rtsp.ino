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
#define USE_RTSP_LOGIN // activate RTSP login support
#include "AudioTools/Communication/RTSP.h"

int port = 554;
AudioInfo info(16000,1,16); // AudioInfo for RTSP
const char* wifi = "ssid";
const char* password = "password";

SineFromTable<int16_t> sineWave(32000);           // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);    // Stream generated from sine wave
RTSPAudioSource source(sound, info);              // Stream sound via RTSP
RTSPAudioStreamer<RTSPPlatformWiFi> streamer(source);
RTSPServer<RTSPPlatformWiFi> rtsp(streamer, port);


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // Setup sine wave
  auto cfgS = sineWave.defaultConfig();
  cfgS.copyFrom(info);
  sineWave.begin(cfgS, N_B4);

  // Start Wifi & rtsp server
  rtsp.begin(wifi, password);
}

void loop() { delay(1000); }