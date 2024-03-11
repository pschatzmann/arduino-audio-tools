/**
 * @file streams-audiokit-csv.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/streams-audiokit-serial/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "AudioTools.h"
#include "AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 2, 16);
AudioBoardStream kit(AudioKitEs8388V1); // Access I2S as stream
CsvOutput<int16_t> csvStream(Serial);
StreamCopy copier(csvStream, kit); // copy kit to csvStream

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Warning);
    
    auto cfg = kit.defaultConfig(RX_MODE);
    cfg.copyFrom(info);
    cfg.input_device = ADC_INPUT_LINE2;
    kit.begin(cfg);

    // make sure that we have the correct channels set up
    csvStream.begin(info);

}

// Arduino loop - copy data
void loop() {
    copier.copy();
}
