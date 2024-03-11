/**
 * @file streams-i2s_pdm-serial.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-i2s_pdm-serial/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "AudioTools.h"

AudioInfo info(44100, 1, 16);
I2SStream i2sStream; // Access I2S as stream
CsvOutput<int16_t> csvStream(Serial);
StreamCopy copier(csvStream, i2sStream); // copy i2sStream to csvStream

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);
    
    auto cfg = i2sStream.defaultConfig(RX_MODE);
    cfg.copyFrom(info);
    cfg.signal_type = PDM;
    cfg.channel_format = I2S_CHANNEL_FMT_ALL_RIGHT;  // try with left
    cfg.use_apll = false;  
    cfg.auto_clear = false;
    cfg.pin_bck = I2S_PIN_NO_CHANGE; // not used
    i2sStream.begin(cfg);

    // make sure that we have the correct channels set up
    csvStream.begin(info);

}

// Arduino loop - copy data
void loop() {
    copier.copy();
}
