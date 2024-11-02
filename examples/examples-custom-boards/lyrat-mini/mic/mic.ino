

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 1, 16);
AudioBoardStream i2s(LyratMini); // Access I2S as stream
CsvOutput<int16_t> csvStream(Serial);
StreamCopy copier(csvStream, i2s); // copy i2s to csvStream

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);
    
    auto cfg = i2s.defaultConfig(RX_MODE);
    cfg.copyFrom(info);
    // cfg.i2s_function = CODEC; // or CODEC_ADC
    i2s.begin(cfg);

    // make sure that we have the correct number of channels set up
    csvStream.begin(info);

}

// Arduino loop - copy data
void loop() {
    copier.copy();
}