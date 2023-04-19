#include "AudioTools.h"
#include "AudioLibs/RTSPStream.h"
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
RTSPStream rtsp_stream(format, encoder);
StreamCopy copier(rtsp_stream, sound);  // rtsp to sine
// Server
RTSPServer rtsp(rtsp_stream.streamer(), port);


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Start Wifi & rtsp server
  rtsp.begin(wifi, password);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // Start Output Stream
  rtsp_stream.begin(info);

}

void loop() {
  if (rtsp_stream) {
      copier.copy();
  }
}