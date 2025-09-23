/**
 * @file communication-audiokit-rtsp.ino
 * @author Phil Schatzmann
 * @brief Provide Microphone from AudioKit via RTSP. Depends on https://github.com/pschatzmann/Micro-RTSP-Audio
 * @version 0.1
 * @date 2022-05-02
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#define USE_RTSP_LOGIN // activate RTSP login support
#include "AudioTools/Communication/RTSP.h"

int port = 554;
AudioBoardStream kit(AudioKitEs8388V1);  // Audio source
RTSPAudioSource source(kit); // IAudioSource for RTSP
RTSPAudioStreamer<RTSPPlatformWiFi> streamer(source); // Stream audio via RTSP
RTSPServer<RTSPPlatformWiFi> rtsp(streamer, port);

const char* wifi = "wifi";
const char* password = "password";

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup Audiokit as source
  auto cfg = kit.defaultConfig(RX_MODE);
  cfg.input_device = ADC_INPUT_LINE2;
  cfg.channels = 1;
  cfg.sample_rate = 8000;
  cfg.bits_per_sample = 16;
  kit.begin(cfg);

  // Start Wifi
  rtsp.begin(wifi, password);

}

void loop() { delay(1000); }