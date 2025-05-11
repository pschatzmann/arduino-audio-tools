#include "AudioTools.h"
#include "AudioTools/Communication/UDPStream.h"
#include "AudioTools/Communication/AOO.h"

const char *ssid = "ssid";
const char *password = "password";
const int udpPort = 7000;
AudioInfo info(22000, 1, 16);
SineWaveGenerator<int16_t> sineWave(32000);     // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);  // Stream generated from sine wave
UDPStream udp(ssid, password);
IPAddress udpAddress(192, 168, 1, 255);
AOOSource aoo_source(udp);
StreamCopy copier(aoo_source, sound);  // copies sound into i2s

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // Define udp address and port
  udp.begin(udpAddress, udpPort);

  // start aoo
  aoo_source.begin(info);

  Serial.println("started...");
}

void loop() {
  copier.copy();
}