#include "AudioTools.h"
#include "AudioLibs/RTSPStream.h"
#include "AudioStreamer.h"
#include "RTSPServer.h"

int port = 554;
AudioInfo info(32000, 2, 16);
const char* wifi = "ssid";
const char* password = "password";

// Sine tone generator
SineFromTable<int16_t> sineWave(32000);           // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);    // Stream generated from sine wave
// setup RTSP Stream
RTSPFormatSBC format;
SBCDecoder decoder;
RTPStream rtsp_stream(format, decoder);
StreamCopy copier(rtsp_stream, sound);  // copy mic to tfl
// Server
RTSPServer rtsp = RTSPServer(stream.audioSource(), port);


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // Start Output Stream
  rtsp_stream.begin(info);

  // Start Wifi & rtsp server
  rtsp.begin(wifi, password);
}

void loop() { 
    copier.copy();
 }