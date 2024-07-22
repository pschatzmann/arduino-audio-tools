/**
 * @file mp3-BufferRTOS.ino
 * @author Phil Schatzmann
 * @brief Performance test for mp3 decoding form SD on core 1 and pass the data
 * to core 0;
 * @version 0.1
 * @date 2022-12-06
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioLibs/AudioBoardStream.h"
#include "AudioLibs/AudioSourceSDFAT.h"
#include "AudioLibs/Concurrency.h"

const int buffer_count = 30;
const int buffer_size = 512;
const char *startFilePath = "/";
const char *ext = "mp3";
AudioSourceSDFAT source(startFilePath, ext, PIN_AUDIO_KIT_SD_CARD_CS);
MP3DecoderHelix decoder;
BufferRTOS<uint8_t> buffer(buffer_size *buffer_count, buffer_size,
                           portMAX_DELAY, 10);  // fast synchronized buffer
QueueStream<uint8_t> out(buffer);               // convert Buffer to Stream
AudioPlayer player(source, out, decoder);
int32_t get_data(uint8_t *data, int32_t bytes);
uint8_t data[buffer_size];

// Performance measurement: Receive data on core 0
Task task("name", 10000, 10, 0);

// Provide data to A2DP
int32_t get_data(uint8_t *data, int32_t bytes) {
  size_t result_bytes = buffer.readArray(data, bytes);
  // LOGI("get_data_channels %d -> %d of (%d)", bytes, result_bytes ,
  // buffer.available());
  return result_bytes;
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // sd_active is setting up SPI with the right SD pins by calling
  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO,
            PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);

  // start QueueStream
  out.begin();

  // setup player
  player.setDelayIfOutputFull(0);
  player.setVolume(0.1);
  player.begin();

  // fill buffer with some data
  player.getStreamCopy().copyN(5);

  // start the task on kernel 0
  task.begin([]() {
    uint64_t start = millis();
    size_t bytes = 0;
    for (int i = 0; i < 100; i++) {
      bytes += get_data(data, buffer_size);
      yield();
    }
    uint64_t timeMs = millis() - start;
    // prevent div by 0
    if (timeMs == 0) {
      timeMs = 1;
    }
    // calculate samples per second
    size_t samples_per_second = bytes / timeMs * 1000 / 4;
    Serial.print("Samples per second: ");
    Serial.println(samples_per_second);
  });
}

void loop() {
  // decode data to buffer
  player.copy();
}