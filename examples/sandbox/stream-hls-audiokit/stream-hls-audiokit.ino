/**
 * @file stream-hls-audiokit.ino
 * @brief Example for HLSStream:
 * The HLSStream class is parsing request url in order to determine the ts urls, which are
 * automatically loaded an their content is provided in sequence. The content type is usually
 * MPEG-TS (MTS). We can extract the audio with the help of a MTSDecoder in order to get e.g 
 * ADTS encded AAC data 
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/HLSStream.h"
#include "AudioTools/AudioCodecs/CodecMTS.h"
#include "AudioTools/AudioCodecs/CodecADTS.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"


AudioBoardStream out(AudioKitEs8388V1);// final output of decoded stream
HLSStream hls_stream("SSID", "password");
MTSDecoder mts;
ADTSDecoder adts;
AACDecoderHelix aac;
EncodedAudioStream aac_stream(&out, &aac); 
EncodedAudioStream adts_stream(&aac_stream, &adts);
EncodedAudioStream mts_stream(&adts_stream, &mts);
StreamCopy copier(mts_stream, hls_stream);

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // make it a bit less chatty
  adts_stream.setLogLevel(AudioLogger::Warning);

  // for some strange reasons this is reporting different sampling rates
  // so we switch it off
  aac.setAudioInfoNotifications(false);

  // we set the fixed sampling rate
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.sample_rate = 48000;
  out.begin(cfg);

  mts_stream.begin();
  aac_stream.begin();

  hls_stream.begin("http://a.files.bbci.co.uk/media/live/manifesto/audio/simulcast/hls/nonuk/sbr_vlow/ak/bbc_world_service.m3u8");
}

// Arduino loop  
void loop() {
  copier.copy();
}
