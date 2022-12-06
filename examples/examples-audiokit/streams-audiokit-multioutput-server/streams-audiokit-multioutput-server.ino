/**
 * @file streams-audiokit-multioutput-server.ino
 * @author Phil Schatzmann
 * @brief MultiOutput Example using output to speaker and webserver
 * @version 0.1
 * @date 2022-07-26
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

const int buffer_count = 10;
const int buffer_size = 1024;
AudioKitStream kit; // input & output
QueueStream<uint8_t> queue(buffer_size, buffer_count, true); 
AudioEncoderServer server(new WAVEncoder(),"WIFI","password");  
MultiOutput out(queue, kit);
StreamCopy copier(out, kit); // copy kit to kit

// Arduino Setup
void setup(void) {
   Serial.begin(115200);
   AudioLogger::instance().begin(Serial, AudioLogger::Info);

   // setup audiokit
   auto cfg = kit.defaultConfig(RXTX_MODE);
   cfg.sd_active = false;
   cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2; // input from microphone
   cfg.sample_rate = 16000;
   kit.setVolume(0.5);
   kit.begin(cfg);

   // start queue
   queue.begin();

   // start server
   server.begin(queue, cfg);
   Serial.println("started...");

}

// Arduino loop - copy data
void loop() {
   copier.copy();
   server.copy();
}

