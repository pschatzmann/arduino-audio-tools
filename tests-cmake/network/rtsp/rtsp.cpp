#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSTD.h"
#include "AudioTools/AudioCodecs/MP3Parser.h"
#include "AudioTools/Communication/RTSP/RTSPPlatformWiFi.h"
#include "AudioTools/Communication/RTSP.h"

int port = 8554;

// rtsp
MP3ParserEncoder enc; // mp3 packaging
RTSPFormatMP3 mp3format(enc); // RTSP mp3
MetaDataFilterEncoder filter(enc);
RTSPOutput<RTSPPlatformWiFi> rtsp_out(mp3format, filter);
AudioSourceSTD source("/home/pschatzmann/Music/Elvis Costello/Best Of/", ".mp3");
CopyDecoder dec; // no decoding, just copy
AudioPlayer player(source, rtsp_out, dec);
RTSPServer<RTSPPlatformWiFi> rtsp(rtsp_out.streamer(), port);


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // delay between mp3 files
  source.setTimeoutAutoNext(1000);

  // start the player
  player.begin();

  // Start Output Stream
  rtsp_out.begin();

  // Start Wifi & rtsp server
  rtsp.begin();

}

void loop() {
  if (rtsp_out && rtsp) {
    player.copy();
  }
}
