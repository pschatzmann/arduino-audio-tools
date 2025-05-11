/***
 * @brief We receive audio data using Audio Over OSC (AOO) via UDP.
 * We support the decoding of pcm and opus.
 * The audio output is done to the I2SStream.
 */

#include "AudioTools.h"
#include "AudioTools/Communication/AOO.h"
#include "AudioTools/Communication/UDPStream.h"
#include "AudioTools/AudioCodecs/CodecOpus.h" // https://github.com/pschatzmann/arduino-libopus

// #include "AudioTools/AudioLibs/AudioBoardStream.h"

const char* ssid = "SSID";
const char* password = "password";
UDPStream udp(ssid, password);
const int udpPort = 7000;
I2SStream out;  // or ony other e.g. AudioBoardStream
AudioInfo info(22000, 2, 16);
AOOSink aoo_sink(udp, out);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // start UDP receive
  udp.begin(udpPort);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.buffer_size = 1024;
  config.buffer_count = 20;
  out.begin();

  // setup the AOOSink
  aoo_sink.addDecoder("opus",[]() { return (AudioDecoder *)new OpusAudioDecoder(); });
  aoo_sink.begin(info);

  Serial.println("started...");
}

void loop() {
  aoo_sink.copy();
}
