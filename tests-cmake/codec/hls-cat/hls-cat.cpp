/**
 * Test sketch for hls decoding: It seems that the playback issue is coming from
 * the MTSDecoder. It needs to be reset after each file?
*/
#include "AudioTools.h"
#include "AudioCodecs/CodecMTS.h"
#include "AudioCodecs/CodecADTS.h"
#include "AudioCodecs/CodecAACHelix.h"
#include "AudioLibs/PortAudioStream.h"
#include "AudioLibs/Desktop/File.h"

AudioInfo info(48000,2,16);
CatStream hls_stream;
PortAudioStream out;
MTSDecoder mts;
ADTSDecoder adts;
AACDecoderHelix aac;
EncodedAudioStream aac_stream(&out, &aac); 
EncodedAudioStream adts_stream(&aac_stream, &adts);
EncodedAudioStream mts_stream(&adts_stream, &mts);
StreamCopy copier(mts_stream, hls_stream);

// Arduino Setup
void setup(void) {
  //Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);
  //hls_stream.setLogLevel(AudioLogger::Debug); // hls_stream is quite chatty at Info
  adts_stream.setLogLevel(AudioLogger::Debug);
  mts_stream.setLogLevel(AudioLogger::Debug);
  hls_stream.add(new File("/home/pschatzmann/Downloads/7081.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7082.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7083.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7084.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7085.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7086.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7087.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7088.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7089.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7090.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7091.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7092.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7093.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7094.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7095.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7096.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7097.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7098.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7099.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7100.ts"));
  hls_stream.add(new File("/home/pschatzmann/Downloads/7101.ts"));

  aac.setAudioInfoNotifications(false);

  auto cfg = out.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  out.begin();

  mts_stream.begin();
  aac_stream.begin();
  adts_stream.begin();

  hls_stream.begin();
  Serial.println("playing...");
}

// Arduino loop  
void loop() {
  copier.copy();
}
