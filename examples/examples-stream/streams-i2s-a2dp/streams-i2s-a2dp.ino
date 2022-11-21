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
A2DPStream a2dpStream;                          // access A2DP as stream
VolumeStream volume(a2dpStream);
StreamCopy copier(volume, i2sStream); // copy i2sStream to a2dpStream
ConverterFillLeftAndRight<int16_t> filler(LeftIsEmpty); // fill both channels

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    // set intial volume
    volume.setVolume(0.3);
    
    // start bluetooth
    Serial.println("starting A2DP...");
    auto cfgA2DP = a2dpStream.defaultConfig(TX_MODE);
    cfgA2DP.name = "LEXON MINO L";
    a2dpStream.begin(cfgA2DP);

    // start i2s input with default configuration
    Serial.println("starting I2S...");
    a2dpStream.setNotifyAudioChange(i2sStream); // i2s is using the info from a2dp
    i2sStream.begin(i2sStream.defaultConfig(RX_MODE));

}

// Arduino loop - copy data
void loop() {
   // copier.copy(filler);
    copier.copy();
}