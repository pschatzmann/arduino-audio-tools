/*
 * Desktop RTSP test: streams a generated sine tone as raw PCM (L16).
 *
 * Ported from examples/examples-communication/rtsp/audio-server-generator
 * for desktop use (via the Arduino-Emulator) - no WiFi credentials needed,
 * fixed non-privileged port instead of 554.
 *
 * Connect with: ffplay -rtsp_transport tcp rtsp://127.0.0.1:8554/sine
 */
#include "AudioTools.h"
#include "AudioTools/Communication/RTSP/RTSPPlatformWiFi.h"
#include "AudioTools/Communication/RTSP.h"

using namespace audio_tools;

const int port = 8554;
AudioInfo info(16000, 1, 16);  // AudioInfo for RTSP

SineFromTable<int16_t> sineWave(32000);          // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);   // Stream generated from sine wave
RTSPMediaSource source(sound, info);             // Stream sound via RTSP
RTSPMediaStreamer<RTSPPlatformWiFi> streamer(source);
RTSPServer<RTSPPlatformWiFi> rtsp(streamer, port);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // Setup sine wave
  auto cfgS = sineWave.defaultConfig();
  cfgS.copyFrom(info);
  sineWave.begin(cfgS, N_B4);

  rtsp.begin();
}

void loop() { delay(1000); }
