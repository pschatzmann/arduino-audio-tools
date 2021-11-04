/**
 * @file streams-STK-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-STK-a2dp/README.md
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

STKStream stkStream;                            // Access I2S as stream
A2DPStream a2dpStream = A2DPStream::instance(); // access A2DP as stream
StreamCopy copier(a2dpStream, stkStream); // copy stkStream to a2dpStream
ConverterFillLeftAndRight<int16_t> filler(RightIsEmpty); // fill both channels

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);
    
    // start bluetooth
    Serial.println("starting A2DP...");
    auto cfgA2DP = a2dpStream.defaultConfig(TX_MODE);
    cfgA2DP.name = "LEXON MINO L";
    a2dpStream.setNotifyAudioChange(stkStream); // i2s is using the info from a2dp
    a2dpStream.begin(cfgA2DP);

    // start i2s input with default configuration
    Serial.println("starting I2S...");
    stkStream.begin(stkStream.defaultConfig(RX_MODE));

}

// Arduino loop - copy data
void loop() {
    copier.copy(filler);
}
