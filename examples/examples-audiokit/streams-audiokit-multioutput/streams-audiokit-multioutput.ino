/**
 * @file streams-audiokit-multioutput.ino
 * @author Phil Schatzmann
 * @brief MultiOutput Example using add
 * @version 0.1
 * @date 2022-07-26
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

AudioKitStream kit; // Access I2S as stream
CsvStream<int16_t> csv(Serial,2);
MultiOutput out;
StreamCopy copier(out, kit); // copy kit to kit

// Arduino Setup
void setup(void) {
    Serial.begin(230400);
    AudioLogger::instance().begin(Serial, AudioLogger::Warning);

    out.add(csv);
    out.add(kit);
    
    auto cfg = kit.defaultConfig(RXTX_MODE);
    cfg.sd_active = false;
    cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2; // input from microphone
    cfg.sample_rate = 8000;
    kit.setVolume(0.5);
    kit.begin(cfg);
}

// Arduino loop - copy data
void loop() {
    copier.copy();
}