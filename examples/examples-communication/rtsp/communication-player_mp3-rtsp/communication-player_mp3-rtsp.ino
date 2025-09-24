
// Use the AudioPlayer to publish mp3 data as-is

#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSDMMC.h"
#include "AudioTools/AudioCodecs/MP3Parser.h"
#define USE_RTSP_LOGIN // activate RTSP login support
#include "AudioTools/Communication/RTSP.h"

int port = 554;
const char* wifi = "SSID";
const char* password = "password";

// rtsp
MP3ParserEncoder enc; // mp3 packaging
RTSPFormatMP3 mp3format(enc); // RTSP mp3
MetaDataFilterEncoder filter(enc);
RTSPOutput<RTSPPlatformWiFi> rtsp_out(mp3format, filter);
AudioSourceSDMMC source("/", ".mp3");
CopyDecoder dec; // no decoding, just copy
AudioPlayer player(source, rtsp_out, dec);
RTSPServer<RTSPPlatformWiFi> rtsp(rtsp_out.streamer(), port);


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // delay between mp3 files
  source.setTimeoutAutoNext(1000);

  // start the player
  player.begin();

  // Start Output Stream
  rtsp_out.begin();

  // Start Wifi & rtsp server
  rtsp.begin(wifi, password);

}

void loop() {
  if (rtsp_out && rtsp) {
      player.copy();
  }
}
