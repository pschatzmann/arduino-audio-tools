#include "AudioTools.h"
#include "AudioTools/Communication/AOO.h"
#include "AudioTools/Communication/UDPStream.h"
// #include "AudioTools/AudioLibs/AudioBoardStream.h"

const char* ssid = "SSID";
const char* password = "password";
UDPStream udp(ssid, password);
const int udpPort = 7000;
I2SStream out;  // or ony other e.g. AudioBoardStream
AAOSink aao_sink(udp, out);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // start UDP receive
  udp.begin(udpPort);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.buffer_size = 1024;
  config.buffer_count = 20;
  out.begin(config);

  Serial.println("started...");
}

void loop() {
  aao_sink.copy();
}
