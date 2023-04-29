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
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/RTSP.h"
#include "AudioStreamer.h"
#include "RTSPServer.h"

int port = 554;
AudioKitStream kit;  // Audio source
RTSPSourceFromAudioStream source(kit); // IAudioSource for RTSP
AudioStreamer streamer = AudioStreamer(&source); // Stream audio via RTSP
RTSPServer rtsp = RTSPServer(&streamer, port);

const char* wifi = "wifi";
const char* password = "password";

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup Audiokit as source
  auto cfg = kit.defaultConfig(RX_MODE);
  cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
  cfg.channels = 1;
  cfg.sample_rate = 8000;
  cfg.bits_per_sample = 16;
  kit.begin(cfg);

  // Start Wifi
  rtsp.begin(wifi, password);

}

void loop() { delay(1000); }