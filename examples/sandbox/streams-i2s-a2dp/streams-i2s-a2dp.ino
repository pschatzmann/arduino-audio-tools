/**
 * @file streams-i2s-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/i2s-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// Add this in your sketch or change the setting in AudioConfig.h
#define USE_A2DP

#include "Arduino.h"
#include "AudioTools.h"
#include "AudioA2DP.h"

using namespace audio_tools;  

I2SStream i2sStream;    // Access I2S as stream
A2DPStream a2dpStream = A2DPStream::instance(); // access A2DP as stre
StreamCopy copier(a2dpStream, i2sStream); // copy i2sStream to a2dpStream
ConverterFillLeftAndRight<int16_t> filler(RightIsEmpty); // fill both channels

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dpStream.begin(TX_MODE, "MyMusic");  
  Serial.println("A2DP is connected now");

  // start i2s input with default configuration
  Serial.println("starting I2S...");
  auto config = i2sStream.defaultConfig(RX_MODE);
  config.sample_rate = 44100;
  config.channels = 2;
  config.bits_per_sample = 16;
  i2sStream.begin(config);
  Serial.println("I2S started");

}

// Arduino loop - copy data 
void loop() {
    if (a2dpStream){
        copier.copy(filler);
    }
}
