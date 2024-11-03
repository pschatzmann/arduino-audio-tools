
/**
 * @file output.ino
 * @author Phil Schatzmann
 * @brief  Demo how to use the microphone.
 * @version 0.1
 * @date 2024-11-03
 *
 * The microphone seems to be attached to 2 different i2s ports. In addition to the
 * ES8311, the ES7243 is also started. 
 * The I2S pins can be selected via cfg.i2s_function: CODEC uses the ES8311 I2S pins 
 * and CODEC_ADC uses the ES7243 I2S pins; By default the CODEC value is used.
 *
 * Only CODEC_ADC will give a proper microphone input! 
 * 
 * @copyright Copyright (c) 2022
 */


#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 2, 16);
AudioBoardStream i2s(LyratMini); // Access I2S as stream
CsvOutput<int16_t> csvStream(Serial);
StreamCopy copier(csvStream, i2s); // copy i2s to csvStream

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    // Display details what is going on
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
    AudioDriverLogger.begin(Serial, AudioDriverLogLevel::Info);
    
    auto cfg = i2s.defaultConfig(RX_MODE);
    cfg.copyFrom(info);
    // cfg.i2s_function = PinFunction::CODEC_ADC;  // determined automatically
    // cfg.i2s_port_no = 0; // or 1 if 0 is already in use
    i2s.begin(cfg);

    // make sure that we have the correct number of channels set up
    csvStream.begin(info);

}

// Arduino loop - copy data
void loop() {
    copier.copy();
}