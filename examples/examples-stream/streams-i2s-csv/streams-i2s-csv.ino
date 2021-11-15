/**
 * @file streams-i2s-csv.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/stream-i2s-csv/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "AudioTools.h"

using namespace audio_tools;  
ConverterSwapBytes<int32_t> swap;
ConverterOutlierFilter<int32_t> outlier;
MultiConverter<int32_t> mc(outlier);

I2SStream i2sStream; // Access I2S as stream
CsvStream<int32_t> csvStream(Serial);
//HexDumpStream csvStream(Serial);
StreamCopy copier(csvStream, i2sStream); // copy i2sStream to csvStream

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);
    
    auto cfg = i2sStream.defaultConfig(RX_MODE);
    cfg.bits_per_sample = 32;
    cfg.channels = 2;
    cfg.sample_rate = 96000;
    cfg.is_master = false;
    cfg.i2s_format = I2S_MSB_FORMAT;
    i2sStream.begin(cfg);

    // make sure that we have the correct channels set up
    csvStream.begin(cfg);

}

// Arduino loop - copy data
void loop() {
    copier.copy(mc);
}
