#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMTS.h"
#include "AudioTools/AudioCodecs/CodecADTS.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
//#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/AudioLibs/MiniAudioStream.h"
#include "AudioTools/AudioLibs/HLSStream.h"

AudioInfo info(48000,2,16);
HLSStream hls_stream("NA", "NA");
// HexDumpOutput hex(Serial);
// NullStream null;
//CsvOutput<int16_t> out(Serial, 2);  // Or use StdOuput
//PortAudioStream out;
MiniAudioStream out;
AACDecoderHelix aac;
CodecMTS mts{aac};
// ADTSDecoder adts;
// AACDecoderHelix aac;
// EncodedAudioStream aac_stream(&out, &aac); 
// EncodedAudioStream adts_stream(&aac_stream, &adts);
EncodedAudioStream mts_stream(&out, &mts);
StreamCopy copier(mts_stream, hls_stream);

// Arduino Setup
void setup(void) {
  //Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  //hls_stream.setLogLevel(AudioLogger::Debug); // hls_stream is quite chatty at Info
  //adts_stream.setLogLevel(AudioLogger::Debug);
  //mts_stream.setLogLevel(AudioLogger::Debug);

  //aac.setAudioInfoNotifications(false);

  auto cfg = out.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  out.begin();

  mts_stream.begin();
  // aac_stream.begin();
  // adts_stream.begin();

  hls_stream.begin("http://a.files.bbci.co.uk/media/live/manifesto/audio/simulcast/hls/nonuk/sbr_vlow/ak/bbc_world_service.m3u8");
  Serial.println("playing...");
}

// Arduino loop  
void loop() {
  copier.copy();
}
