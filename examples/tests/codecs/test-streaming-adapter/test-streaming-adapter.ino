#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

URLStream url("ssid","password");  // or replace with ICYStream to get metadata
AudioBoardStream i2s(AudioKitEs8388V1); // final output of decoded stream
MP3DecoderHelix helix;
StreamingDecoderAdapter decoder(helix);

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  i2s.begin(config);

  // setup I2S based on sampling rate provided by decoder
  decoder.setInput(url);
  decoder.setOutput(i2s);
  decoder.begin();

  // mp3 radio
  url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128","audio/mp3");

}

void loop(){
  decoder.copy();
}