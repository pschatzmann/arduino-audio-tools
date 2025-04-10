/**
 * @file streams-audiokit-csv.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/streams-audiokit-serial/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 2, 16);
AudioBoardStream kit(AudioKitEs8388V1); // Access I2S as stream
CsvOutput<int16_t> csvOutput(Serial);
StreamCopy copier(csvOutput, kit); // copy kit to csvOutput

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);
    
    auto cfg = kit.defaultConfig(RX_MODE);
    cfg.copyFrom(info);
    cfg.input_device = ADC_INPUT_LINE2;
    kit.begin(cfg);

    // make sure that we have the correct channels set up
    csvOutput.begin(info);

}

// Arduino loop - copy data
void loop() {
    copier.copy();
}
