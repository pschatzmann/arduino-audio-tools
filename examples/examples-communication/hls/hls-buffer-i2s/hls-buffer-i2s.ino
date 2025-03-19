/**
 * @file hls-buffer-i2s.ino
 * @brief Buffered playback of HLSStream: activate PSRAM!
 * We use a MultiDecoder to handle different formats.
 * For MPEG-TS (MTS) you need to set the log level to Warning or higher.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/AudioCodecs/CodecMTS.h"
#include "AudioTools/AudioLibs/HLSStream.h"
#include "AudioTools/Concurrency/RTOS.h"
// #include "AudioTools/AudioLibs/AudioBoardStream.h"

// AudioBoardStream out(AudioKitEs8388V1);  // final output of decoded stream
I2SStream out;  // audio output
BufferRTOS<uint8_t> buffer(0);
QueueStream<uint8_t> queue(buffer);
HLSStream hls_stream("ssid", "password");  // audio data source
// decoder
MP3DecoderHelix mp3;
AACDecoderHelix aac;
MTSDecoder mts(aac);             // MPEG-TS (MTS) decoder
MultiDecoder multi(hls_stream);  // MultiDecoder using mime from hls_stream
EncodedAudioOutput dec(&out, &multi);
// 2 separate copy processes
StreamCopy copier_play(dec, queue);
StreamCopy copier_write_queue(queue, hls_stream);
Task writeTask("write", 1024 * 8, 10, 0);

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // https://streams.radiomast.io/ref-128k-mp3-stereo/hls.m3u8
  // https://streams.radiomast.io/ref-128k-aaclc-stereo/hls.m3u8
  // https://streams.radiomast.io/ref-64k-heaacv1-stereo/hls.m3u8
  if (!hls_stream.begin(
          "https://streams.radiomast.io/ref-128k-mp3-stereo/hls.m3u8"))
    stop();

  // register decoders with mime types
  multi.addDecoder(mp3, "audio/mpeg");
  multi.addDecoder(aac, "audio/aac");
  multi.addDecoder(mts, "video/MP2T");  // MPEG-TS

  // start output
  auto cfg = out.defaultConfig(TX_MODE);
  // add optional configuration here: e.g. pins
  out.begin(cfg);

  buffer.resize(10 * 1024);  // increase to 50k psram
  dec.begin();               // start decoder
  queue.begin(100);          // activate read when 100% full

  writeTask.begin([]() { copier_write_queue.copy(); });
}

// Arduino loop
void loop() { copier_play.copy(); }
