/**
 * @file url_mts-hex.ino
 * @author Phil Schatzmann

 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioCodecs/CodecMTS.h"
#include "AudioLibs/HLSStream.h"

HexDumpOutput out(Serial);                                   
HLSStream hls_stream("SSID", "password");
MTSDecoder mts;
EncodedAudioStream mts_stream(&out, &mts);
StreamCopy copier(mts_stream, hls_stream);

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  mts_stream.begin();

  hls_stream.begin("http://a.files.bbci.co.uk/media/live/manifesto/audio/simulcast/hls/nonuk/sbr_vlow/ak/bbc_world_service.m3u8");

}

// Arduino loop  
void loop() {
  copier.copy();
}