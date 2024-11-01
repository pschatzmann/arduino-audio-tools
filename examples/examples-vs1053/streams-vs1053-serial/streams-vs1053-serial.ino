/**
 * @file streams-vs1053-serial.ino
 * @author Phil Schatzmann
 * @brief Reads audio data from the VS1053 microphone
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "AudioTools.h"
#include "AudioTools/AudioLibs/VS1053Stream.h"

AudioInfo info(1600, 1, 16);
VS1053Stream in; // Access VS1053/VS1003 as stream
CsvOutput<int16_t> csvStream(Serial);
StreamCopy copier(csvStream, in); // copy in to csvStream

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);
    
    auto cfg = in.defaultConfig(RX_MODE);
    cfg.copyFrom(info);
    cfg.input_device = VS1053_MIC; // or VS1053_AUX
    in.begin(cfg);

    // make sure that we have the correct channels set up
    csvStream.begin(info);

}

// Arduino loop - copy data
void loop() {
    copier.copy();
}
