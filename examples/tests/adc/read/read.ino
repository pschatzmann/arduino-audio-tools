#include "AudioTools.h"
#include "AudioTools.h"

AudioInfo info(44100, 1, 16);
AnalogAudioStream adc; 
MeasuringStream out(10, &Serial);
StreamCopy copier(out, adc); 

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    LOGI("Supported samples rates: %d - %d", SOC_ADC_SAMPLE_FREQ_THRES_LOW, SOC_ADC_SAMPLE_FREQ_THRES_HIGH)
    
    auto cfg = adc.defaultConfig(RX_MODE);
    cfg.copyFrom(info);
    //cfg.use_apll = false;  // try with yes
    if (!adc.begin(cfg)) {
      LOGE("adc.begin() failed");
      stop();
    } 

    // make sure that we have the correct channels set up
    out.begin(info);
}

// Arduino loop - copy data
void loop() {
    copier.copy();
}