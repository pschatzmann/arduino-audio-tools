/**
 * @file base-player-a2dp.ino
 * @author Phil Schatzmann
 * @brief Test which uses the A2DP callback to provide data from the AudioPlayer
 * 
 * @version 0.1
 * @date 2022-12-04
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "AudioLibs/AudioA2DP.h"
#include "AudioLibs/AudioSourceSDFAT.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioLibs/SynchronizedBuffer.h"
#include "AudioLibs/AudioKit.h"

int buffer_count = 30;
int buffer_size = 512;
const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSDFAT source(startFilePath, ext, PIN_AUDIO_KIT_SD_CARD_CS);
MP3DecoderHelix decoder;
//Setup of synchronized buffer
//audio_tools::Mutex mutex;
//NBuffer<uint8_t> nbuffer(buffer_size, buffer_count);
//RingBuffer<uint8_t> nbuffer(buffer_size*buffer_count);
//SynchronizedBuffer<uint8_t> buffer(nbuffer, mutex);
SynchronizedBufferRTOS<uint8_t> buffer(buffer_size*buffer_count, buffer_size, portMAX_DELAY, 10); // fast synchronized buffer
QueueStream<uint8_t> out(buffer); // convert Buffer to Stream
AudioPlayer player(source, out, decoder);
BluetoothA2DPSource a2dp;

// Provide data to A2DP
int32_t get_data(uint8_t *data, int32_t bytes) {
   size_t result_bytes = buffer.readArray(data, bytes);
   LOGI("get_data_channels %d -> %d of (%d)", bytes, result_bytes , buffer.available());
   return result_bytes;
}

void setup() {
 Serial.begin(115200);
 AudioLogger::instance().begin(Serial, AudioLogger::Warning);

 // sd_active is setting up SPI with the right SD pins by calling 
 SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);

 // start QueueStream
 out.begin();

 // setup player
 player.setDelayIfOutputFull(0);
 player.setVolume(0.1);
 player.begin();
 
 // fill buffer with some data
 player.getStreamCopy().copyN(5); 

 // start a2dp source
 Serial.println("starting A2DP...");
 a2dp.start_raw("LEXON MINO L", get_data);  
 Serial.println("Started!");

}

void loop() {
 // decode data to buffer
 player.copy();
}