/**
 * @file read.ino
 * @author Phil Schatzmann
 * @brief reads the data from the ADC 
 * Default Pins ESP32: 34, 35 (ADC_CHANNEL_6, ADC_CHANNEL_7)
 * Default Pins ESP32C3: 2, 3 (ADC_CHANNEL_2, ADC_CHANNEL_3)
 * @version 0.1
 * @date 2023-11-11
 * 
 * @copyright Copyright (c) 2023
 */ 

#include "AudioTools.h"

AudioInfo info(44100, 2, 16);
AnalogAudioStream adc; 
MeasuringStream out(10, &Serial);
StreamCopy copier(out, adc); 

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

#ifdef ESP32
    LOGI("Supported samples rates: %d - %d", SOC_ADC_SAMPLE_FREQ_THRES_LOW, SOC_ADC_SAMPLE_FREQ_THRES_HIGH);
    LOGI("Supported bit width: %d - %d", SOC_ADC_DIGI_MIN_BITWIDTH, SOC_ADC_DIGI_MAX_BITWIDTH);
#endif
    
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