// a minimal RTSP server example
#include "AudioTools.h"
#include "AudioTools/Communication/RTSP.h"
#include "AudioTestSource.h" 

const char* ssid = "ssid";
const char* password = "password";

int port = 554;
AudioTestSource testSource;
DefaultRTSPAudioStreamer<RTSPPlatformWiFi>  streamer(testSource);
DefaultRTSPServer<RTSPPlatformWiFi>  rtsp(streamer, port);

void setup() {
  Serial.begin(114200);
  rtsp.begin(ssid, password);
}

void loop() { delay(1000); }