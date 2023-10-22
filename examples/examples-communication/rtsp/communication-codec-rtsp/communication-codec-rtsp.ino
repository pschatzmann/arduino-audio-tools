/**
 * @file communication-codec-rtsp.ino
 * @author Phil Schatzmann
 * @brief Provide Audio via RTSP using a codec. Depends on https://github.com/pschatzmann/Micro-RTSP-Audio
 * @version 0.1
 * @date 2022-05-02
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioLibs/RTSP.h"
#include "AudioCodecs/CodecG7xx.h"
#include "RTSPServer.h"

int port = 554;
AudioInfo info(8000, 1, 16); 
const char* wifi = "SSID";
const char* password = "password";

// Sine tone generator
SineFromTable<int16_t> sineWave(32000);         // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);  // Stream generated from sine wave
// rtsp
RTSPFormatG711 format;
G711_ULAWEncoder encoder;
RTSPOutput rtsp_stream(format, encoder);
StreamCopy copier(rtsp_stream, sound);  // rtsp to sine
// Server
RTSPServer rtsp(rtsp_stream.streamer(), port);


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // Start Output Stream
  rtsp_stream.begin(info);

  // Start Wifi & rtsp server
  rtsp.begin(wifi, password);

}

void loop() {
  if (rtsp_stream) {
      copier.copy();
  }
}