/**
 * @file basic-player-a2dp.ino
 * @author Phil Schatzmann
 * @brief Sketch which uses the A2DP callback to provide data from the AudioPlayer via a Queue
 * The queue is filled by the Arduino loop.
 * @version 0.1
 * @date 2022-12-04
 * 
 * @copyright Copyright (c) 2022
 * 
 */

 #include "AudioTools.h"
 #include "AudioTools/AudioLibs/A2DPStream.h"
 #include "AudioTools/Disk/AudioSourceSDFAT.h"
 #include "AudioTools/AudioCodecs/CodecMP3Helix.h"
 //#include "AudioTools/AudioLibs/AudioBoardStream.h"  // for SD Pins
 
 const int cs = 33; //PIN_AUDIO_KIT_SD_CARD_CS;
 const int buffer_size = 15*1024;
 const char *startFilePath = "/";
 const char *ext = "mp3";
 AudioSourceSDFAT source(startFilePath, ext, cs);
 MP3DecoderHelix decoder;
 //Setup of synchronized buffer
 BufferRTOS<uint8_t> buffer(0);
 QueueStream<uint8_t> out(buffer);  // convert Buffer to Stream
 AudioPlayer player(source, out, decoder);
 BluetoothA2DPSource a2dp;
 
 // Provide data to A2DP
 int32_t get_data(uint8_t *data, int32_t bytes) {
   size_t result_bytes = buffer.readArray(data, bytes);
   //LOGI("get_data_channels %d -> %d of (%d)", bytes, result_bytes , buffer.available());
   return result_bytes;
 }
 
 void setup() {
   Serial.begin(115200);
   AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
 
   // allocate in PSRAM only possible in setup or loop
   buffer.resize(buffer_size);
 
   // sd_active is setting up SPI with the right SD pins by calling
   // SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);
 
   // start QueueStream when 95% full
   out.begin(95);
 
   // setup player
   player.setDelayIfOutputFull(0);
   player.setVolume(0.1);
   player.begin();
 
   // start a2dp source
   Serial.println("starting A2DP...");
   a2dp.set_data_callback(get_data);
   a2dp.start("LEXON MINO L");
   Serial.println("Started!");
 }
 
 void loop() {
   // decode data to buffer
   player.copy();
 }