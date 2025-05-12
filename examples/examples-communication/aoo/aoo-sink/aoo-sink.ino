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
I2SStream i2s;  // or ony other e.g. AudioBoardStream i2s(AudioKitEs8388V1);
AOOSink aoo_sink(udp, i2s); // or AOOSinkSingle

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // start UDP receive
  udp.begin(udpPort);

  // start I2S
  Serial.println("starting I2S...");
  auto config = i2s.defaultConfig(TX_MODE);
  i2s.begin(config);

  // setup the AOOSink
  aoo_sink.addDecoder("opus",[]() { return (AudioDecoder *)new OpusAudioDecoder(); });
  aoo_sink.begin();

  Serial.println("started...");
}

void loop() {
  aoo_sink.copy();
}
