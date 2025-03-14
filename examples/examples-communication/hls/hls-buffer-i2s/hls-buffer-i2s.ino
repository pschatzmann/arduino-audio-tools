//
// Buffered playback of HLSStream: activate PSRAM!
// We use a MultiDecoder to handle different formats
//

#include "AudioTools.h"
#include "AudioTools/AudioLibs/HLSStream.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/Concurrency/RTOS.h"
//#include "AudioTools/AudioLibs/AudioBoardStream.h"


//AudioBoardStream out(AudioKitEs8388V1);  // final output of decoded stream
I2SStream out;
BufferRTOS<uint8_t> buffer(0);
QueueStream<uint8_t> queue(buffer);
HLSStream hls_stream("ssid", "password");
// decoder
MP3DecoderHelix mp3;
AACDecoderHelix aac;
MultiDecoder multi(hls_stream); // MultiDecoder using mime from hls_stream
EncodedAudioOutput dec(&out, &multi);

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
  if (!hls_stream.begin("https://streams.radiomast.io/ref-128k-mp3-stereo/hls.m3u8"))
    stop()

  multi.addDecoder(mp3, "audio/mpeg");
  multi.addDecoder(aac, "audio/aac");

  auto cfg = out.defaultConfig(TX_MODE);
  out.begin(cfg);

  buffer.resize(10 * 1024);  // increase to 50k psram
  dec.begin();               // start decoder
  queue.begin(100);          // activate read when 100% full

  writeTask.begin([]() {
    copier_write_queue.copy();
  });
}

// Arduino loop
void loop() {
  copier_play.copy();
}