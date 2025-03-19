/**
 * @file hls-i2s.ino
 * @brief Copy hls stream to decoder: the re-loading of data is causig pauses
 * We use a MultiDecoder to handle different formats.
 * For MPEG-TS (MTS) you need to set the log level to Warning or higher.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/HLSStream.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
//#include "AudioTools/AudioLibs/AudioBoardStream.h"

//AudioBoardStream out(AudioKitEs8388V1);
I2SStream out;
HLSStream hls_stream("ssid", "password");
MP3DecoderHelix mp3;
AACDecoderHelix aac;
MultiDecoder multi(hls_stream); // MultiDecoder using mime from hls_stream
EncodedAudioOutput dec(&out, &multi);
StreamCopy copier(dec, hls_stream);

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // https://streams.radiomast.io/ref-128k-mp3-stereo/hls.m3u8
  // https://streams.radiomast.io/ref-128k-aaclc-stereo/hls.m3u8
  // https://streams.radiomast.io/ref-64k-heaacv1-stereo/hls.m3u8
  if (!hls_stream.begin("https://streams.radiomast.io/ref-128k-mp3-stereo/hls.m3u8"))
    stop();

  multi.addDecoder(mp3, "audio/mpeg");
  multi.addDecoder(aac, "audio/aac");

  auto cfg = out.defaultConfig(TX_MODE);
  out.begin(cfg);

  dec.begin();               // start decoder
}

// Arduino loop
void loop() {
  copier.copy();
}