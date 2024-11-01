// FDK AAC decoding test: psram must be active

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecAACFDK.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

SET_LOOP_TASK_STACK_SIZE(50 * 1024);  

URLStream url("ssid","password");  // or replace with ICYStream to get metadata
AudioBoardStream i2s(AudioKitEs8388V1); // final output of decoded stream
EncodedAudioStream dec(&i2s, new AACDecoderFDK()); // Decoding stream
StreamCopy copier(dec, url); // copy url to decoder


void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  config.buffer_size = 1024;
  config.buffer_count = 20;
  i2s.begin(config);

  // setup I2S based on sampling rate provided by decoder
  dec.begin();

  // aac radio
  url.begin("http://peacefulpiano.stream.publicradio.org/peacefulpiano.aac","audio/aac");

}

void loop(){
  copier.copy();
}
