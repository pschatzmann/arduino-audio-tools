/**
 * @file streams-audiokit-audiokit.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/streams-audiokit-audiokit/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "AudioTools.h"
#include "AudioLibs/AudioBoardStream.h"

AudioBoardStream kit(AudioKitEs8388V1); // Access I2S as stream
StreamCopy copier(kit, kit); // copy kit to kit

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Warning);
    
    auto cfg = kit.defaultConfig(RXTX_MODE);
    cfg.sd_active = false;
    cfg.input_device = ADC_INPUT_LINE2;
    kit.begin(cfg);
}

// Arduino loop - copy data
void loop() {
    copier.copy();
}
