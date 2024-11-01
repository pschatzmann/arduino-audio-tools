/**
 * @file streams-mp34dt05-csv.ino
 * @author Phil Schatzmann
 * @brief Microphone test for Nano BLE Sense which has a MP34DT05 microphone
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioMP34DT05.h"


AudioMP34DT05 mic; // Access I2S as stream
CsvOutput<int16_t> csvStream(Serial);
StreamCopy copier(csvStream, mic); // copy mic to csvStream

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Debug);
    while(!Serial);

    Serial.println("starting...");

    auto cfg = mic.defaultConfig(RX_MODE);
    cfg.bits_per_sample = 16;
    cfg.channels = 1;
    cfg.sample_rate = 16000;
    mic.begin(cfg);

    // make sure that we have the correct channels set up
    csvStream.begin(cfg);

    Serial.println("started");

}

// Arduino loop - copy data
void loop() {
    copier.copy();
}
