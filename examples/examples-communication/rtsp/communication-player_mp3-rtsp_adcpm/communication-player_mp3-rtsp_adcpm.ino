
// Use the AudioPlayer to decode mp3 and publish as adpcm data

#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSDMMC.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioCodecs/CodecADPCM.h"
#define USE_RTSP_LOGIN // activate RTSP login support
#include "AudioTools/Communication/RTSP.h"

int port = 554;
const char* wifi = "SSID";
const char* password = "password";

// mp3 data
AudioSourceSDMMC source("/", ".mp3");
MP3DecoderHelix mp3; // no decoding, just copy
// rtsp
ADPCMEncoder adpcm(AV_CODEC_ID_ADPCM_IMA_WAV, 512); // ima adpcm encoder
RTSPFormatADPCM<ADPCMEncoder> adpcm_format(adpcm); // RTSP adpcm: provide info from encoder
RTSPOutput<RTSPPlatformWiFi> rtsp_out(adpcm_format, adpcm);
FormatConverterStream convert(rtsp_out);
RTSPServer<RTSPPlatformWiFi> rtsp(rtsp_out.streamer(), port);
// player
AudioPlayer player(source, convert, mp3);


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // no delay between mp3 files
  source.setTimeoutAutoNext(0);

  // start the player
  player.begin();

  // Start Output Stream
  rtsp_out.begin();

  // convert the data format
  convert.begin(mp3.audioInfo(), rtsp_out.audioInfo());

  // Start Wifi & rtsp server
  rtsp.begin(wifi, password);

}

void loop() {
  if (rtsp_out) {
      player.copy();
  }
}
