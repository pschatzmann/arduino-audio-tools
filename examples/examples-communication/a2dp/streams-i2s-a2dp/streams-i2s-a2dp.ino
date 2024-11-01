/**
 * @file streams-i2s-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-i2s-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/A2DPStream.h"

I2SStream i2sStream;                            // Access I2S as stream
A2DPStream a2dpStream;                          // access A2DP as stream
StreamCopy copier(a2dpStream, i2sStream); // copy i2sStream to a2dpStream
ConverterFillLeftAndRight<int16_t> filler(LeftIsEmpty); // fill both channels

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
    
    // start bluetooth
    Serial.println("starting A2DP...");
    auto cfgA2DP = a2dpStream.defaultConfig(TX_MODE);
    cfgA2DP.name = "LEXON MINO L";
    a2dpStream.begin(cfgA2DP);

    // set intial volume
    a2dpStream.setVolume(0.3);

    // start i2s input with default configuration
    Serial.println("starting I2S...");
    a2dpStream.addNotifyAudioChange(i2sStream); // i2s is using the info from a2dp
    i2sStream.begin(i2sStream.defaultConfig(RX_MODE));

}

// Arduino loop - copy data
void loop() {
   // copier.copy(filler);
    copier.copy();
}
