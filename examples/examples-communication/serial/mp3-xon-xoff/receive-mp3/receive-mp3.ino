
/**
 * @file receive-mp3.ino
 * @brief Example of receiving an mp3 stream over serial and playing it over I2S
 * using the AudioTools library.
 * The processing implements a xon-xoff flow control over Serial. We
 * process the data receiving in a separate task and the playback in the main
 * loop.
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Concurrency/RTOS.h"

// xon/xoff flow control
const char xon = 17;
const char xoff = 19;
const int min_percent = 10;
const int max_percent = 90;

AudioBoardStream i2s(AudioKitEs8388V1);  // final output of decoded stream
MP3DecoderHelix helix;
EncodedAudioStream dec(&i2s, &helix);  // Decoding stream
// queue
BufferRTOS<uint8_t> buffer(0);
QueueStream<uint8_t> queue(buffer);
// copy
StreamCopy copierFill(queue, Serial1);
StreamCopy copierPlay(dec, queue);

Task task("mp3-copy", 10000, 1, 0);

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  Serial0.begin(115200);

  // set up buffer here to allow PSRAM usage
  buffer.resize(1024 * 10);  // 10kB buffer
  queue.begin(50);           // start when half full

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  i2s.begin(config);

  // setup decoder
  dec.begin();

  // start fill buffer copy task
  task.begin([]() {
    copierFill.copy();
    // data synchronization to prevent buffer overflow
    if (buffer.levelPercent() >= max_percent) {
      Serial1.write(xoff);  // stop receiving
      Serial1.flush();
    } else if (buffer.levelPercent() <= min_percent) {
      Serial1.write(xon);  // start receiving
      Serial1.flush();
    }
  });
}

void loop() { copierPlay.copy(); }
