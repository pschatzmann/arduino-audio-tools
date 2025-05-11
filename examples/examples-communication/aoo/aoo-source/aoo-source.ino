/***
 * @brief We send a sine wave using Audio Over OSC (AOO) using UDP.
 * Since the generation is too fast, we throttle the generated audio
 * to the correct speed.
 */
#include "AudioTools.h"
#include "AudioTools/Communication/AOO.h"
#include "AudioTools/Communication/UDPStream.h"
#include "AudioTools/AudioCodecs/CodecOpus.h"

const char *ssid = "ssid";
const char *password = "password";
const int udpPort = 7000;
AudioInfo info(22000, 1, 16);
SineWaveGenerator<int16_t> sineWave;  
GeneratedSoundStream<int16_t> sound(sineWave);  
UDPStream udp(ssid, password);
IPAddress udpAddress(192, 168, 1, 255);
AOOSource aoo_source(udp);
Throttle throttle(aoo_source);
StreamCopy copier(throttle, sound);  // copies sound into i2s
OpusAudioEncoder opus;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // Define udp address and port
  udp.begin(udpAddress, udpPort);

  // start aoo
  throttle.begin(info);
  //aoo_source.setEncoder(opus); // uncomment comment to use opus
  aoo_source.begin(info);

  Serial.println("started...");
}

void loop() { copier.copy(); }