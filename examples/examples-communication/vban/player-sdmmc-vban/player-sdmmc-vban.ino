/**
 * @file player-sdmmc-vban.ino
 * @author Phil Schatzmann
 * @brief decodes mp3 to vban: We need to use a queue to prevent delays to get too big.
 * We try to keep the queue filled, so that we can always provide pcm data to vban 
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/VBANStream.h"
#include "AudioTools/AudioLibs/AudioSourceSDMMC.h" // or AudioSourceIdxSDMMC.h

const char *startFilePath="/";
const char* ext="mp3";
const int mp3_pcm_size = 14000;
AudioInfo info(44100,2,16);
AudioSourceSDMMC source(startFilePath, ext);
RingBuffer<uint8_t> ring_buffer(mp3_pcm_size*3);
QueueStream<uint8_t> queue(ring_buffer);
MP3DecoderHelix decoder;  // or change to MP3DecoderMAD
AudioPlayer player(source, queue, decoder);

VBANStream out;
StreamCopy copier(out, queue, 2048); // 44100 needs 2048


void fillQueue(){
  // fill queue when smaller then limit
  if(ring_buffer.availableForWrite() >= mp3_pcm_size){
    player.copy();
    // activate copy when limit is reached
    if (!copier.isActive() && ring_buffer.available()>=23000){
      copier.setActive(true);
      LOGI("copier active");
    }
  }
  LOGI("available: %d - avail to write: %d", ring_buffer.available(), ring_buffer.availableForWrite());
}


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // setup output
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  cfg.ssid = "ssid";
  cfg.password = "password";
  cfg.stream_name = "Stream1";
  cfg.target_ip = IPAddress{192,168,1,37};
  cfg.throttle_active = true;
  //cfg.throttle_correction_us = 0;
  if (!out.begin(cfg)) stop();

  // setup player
  player.setVolume(0.7);
  player.begin();

  // select file with setPath() or setIndex()
  //player.setPath("/ZZ Top/Unknown Album/Lowrider.mp3");
  //player.setIndex(1); // 2nd file

  queue.begin(); // start queue
  copier.setActive(false); // deactivate copy

}

void loop() {
  fillQueue();
  // output from queue
  copier.copy();
}