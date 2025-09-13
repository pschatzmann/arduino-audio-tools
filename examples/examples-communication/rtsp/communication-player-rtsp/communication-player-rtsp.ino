
// Use the AudioPlayer to publish mp3 data as-is

#include "AudioTools.h"
#include "AudioTools/AudioLibs/RTSP.h" // https://github.com/pschatzmann/Micro-RTSP-Audio
#include "AudioTools/Disk/AudioSourceSDMMC.h"
#include "RTSPServer.h" 

int port = 554;
const char* wifi = "SSID";
const char* password = "password";

// rtsp
RTSPFormatMP3 mp3format; // RTSP mp3
CopyEncoder enc; // no encoding, just copy
DefaultRTSPOutput rtsp_out(mp3format, enc);
AudioSourceSDMMC source("/", ".mp3");
CopyDecoder dec; // no decoding, just copy
AudioPlayer player(source, rtsp_out, dec);
DefaultRTSPServer rtsp(*rtsp_out.streamer(), port);


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // no delay between mp3 files
  source.setTimeoutAutoNext(0);

  // start the player
  player.begin();

  // Start Output Stream
  rtsp_stream.begin();

  // Start Wifi & rtsp server
  rtsp.begin(wifi, password);

}

void loop() {
  if (rtsp_stream) {
      player.copy();
  }
}
