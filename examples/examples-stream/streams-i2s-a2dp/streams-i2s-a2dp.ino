/**
 * @file streams-i2s-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-i2s-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioLibs/AudioA2DP.h"
I2SStream i2sStream;                            // Access I2S as stream
A2DPStream a2dpStream = A2DPStream::instance(); // access A2DP as stream
StreamCopy copier(a2dpStream, i2sStream); // copy i2sStream to a2dpStream
ConverterFillLeftAndRight<int32_t> filler(RightIsEmpty); // fill both channels
int bits_per_sample = 32;

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);
    
    // start bluetooth
    Serial.println("starting A2DP...");
    auto cfgA2DP = a2dpStream.defaultConfig(TX_MODE);
    cfgA2DP.bits_per_sample = bits_per_sample;
    cfgA2DP.name = "LEXON MINO L";
    a2dpStream.begin(cfgA2DP);
    a2dpStream.setNotifyAudioChange(i2sStream); // i2s is updating sample rate

    // start i2s input with default configuration
    Serial.println("starting I2S...");
    auto cfgI2S = i2sStream.defaultConfig(RX_MODE);
    cfgI2S.bits_per_sample = bits_per_sample;  // not all INMP441 work with 16 bit
    i2sStream.begin(cfgI2S);


}

// Arduino loop - copy data
void loop() {
    copier.copy(filler);
}
