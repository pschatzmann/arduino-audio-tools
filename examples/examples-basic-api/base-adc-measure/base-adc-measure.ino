/**
 * Internal ADC: It depdens pretty much on our processor & arduino version if and how
 * this is supported.
 */

#include "Arduino.h"
#include "AudioTools.h"

// On board analog to digital converter
AnalogAudioStream analog_in; 

// Measure throughput
MeasuringStream measure(1000, &Serial);

// Copy input to output
StreamCopy copier(measure, analog_in);

void setup() {

  delay(3000); // wait for serial to become available

  // Serial Interface
  Serial.begin(115200);
  // Include logging to serial
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info); //Warning, Info, Error, Debug
  
  // Configure ADC input
  auto adcConfig = analog_in.defaultConfig(RX_MODE);

  // For ESP32 by Espressif Systems version 3.0.0 and later 
  // see examples/README_ESP32.md
  // adcConfig.sample_rate = 22050;
  // adcConfig.adc_bit_width = 12;
  // adcConfig.adc_calibration_active = true;
  // adcConfig.is_auto_center_read = false;
  // adcConfig.adc_attenuation = ADC_ATTEN_DB_12; 
  // adcConfig.channels = 2;
  // adcConfig.adc_channels[0] = ADC_CHANNEL_4; 
  // adcConfig.adc_channels[1] = ADC_CHANNEL_5;

  adcConfig.sample_rate = 44100; // per channel

  Serial.println("Starting ADC...");
  analog_in.begin(adcConfig);

  // Start Measurements
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning); //Warning, Debug, Info, Error
  Serial.println("Starting through put measurement...");
}

void loop() {
  copier.copy(); 
}
