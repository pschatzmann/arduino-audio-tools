#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecTSDemux.h"
#include "AudioTools/AudioCodecs/CodecADTS.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/AudioCodecs/CodecMTS.h"
#include "AudioTools/AudioLibs/MiniAudioStream.h"
#include "AudioTools/AudioLibs/HLSStream.h"

AudioInfo info(48000,2,16);
HLSStream hls_stream("NA", "NA");
// HexDumpOutput hex(Serial);
// NullStream null;
//CsvOutput<int16_t> out(Serial, 2);  // Or use StdOuput
MiniAudioStream out;
//MTSDecoder mts;
//ADTSDecoder adts;
AACDecoderHelix aac;
MP3DecoderHelix mp3;
MultiDecoder multi;
//EncodedAudioStream aac_stream(&out, &aac); 
//EncodedAudioStream adts_stream(&aac_stream, &adts);
//EncodedAudioStream mts_stream(&adts_stream, &mts);
EncodedAudioStream dec(&out, &mp3);
StreamCopy copier(dec, hls_stream);

// Arduino Setup
void setup(void) {
  //Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  //hls_stream.setLogLevel(AudioLogger::Debug); // hls_stream is quite chatty at Info
  //adts_stream.setLogLevel(AudioLogger::Debug);
  //mts_stream.setLogLevel(AudioLogger::Debug);

  aac.setAudioInfoNotifications(false);

  auto cfg = out.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  out.begin();

  //mts_stream.begin();
  //aac_stream.begin();
  //adts_stream.begin();

  if (hls_stream.begin("http://audio-edge-cmc51.fra.h.radiomast.io/ref-128k-mp3-stereo/hls.m3u8"))
    Serial.println("playing...");
}

// Arduino loop  
void loop() {
  copier.copy();
}
