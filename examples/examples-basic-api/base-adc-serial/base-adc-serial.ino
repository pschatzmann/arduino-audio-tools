/**
 * @file adc-serial.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/adc-serial/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * #TODO retest is outstanding
 */
 
#include "Arduino.h"
#include "AudioTools.h"

/**
 * @brief We use a mcp6022 analog microphone on GPIO34 and write it to Serial
 */ 

AnalogAudioStream adc;
const int32_t max_buffer_len = 1024;
uint8_t buffer[max_buffer_len];
// The data has a center of around 26427, so we we need to shift it down to bring the center to 0
ConverterScaler<int16_t> scaler(1.0, -26427, 32700 );

// Arduino Setup
void setup(void) {

  delay(3000); // wait for serial to become available

  Serial.begin(115200);
  // Include logging to serial
  AudioLogger::instance().begin(Serial, AudioLogger::Info); //Warning, Info, Error, Debug
  Serial.println("starting ADC...");
  auto adcConfig = adc.defaultConfig(RX_MODE);

  // For ESP32 by Espressif Systems version 3.0.0 and later:
  // see examples/README_ESP32.md
  // adcConfig.sample_rate = 44100;
  // adcConfig.adc_bit_width = 12;
  // adcConfig.adc_calibration_active = true;
  // adcConfig.is_auto_center_read = false;
  // adcConfig.adc_attenuation = ADC_ATTEN_DB_12; 
  // adcConfig.channels = 2;
  // adcConfig.adc_channels[0] = ADC_CHANNEL_4; 
  // adcConfig.adc_channels[1] = ADC_CHANNEL_5;

  adc.begin(adcConfig);
}

// Arduino loop - repeated processing 
void loop() {
  size_t len = adc.readBytes(buffer, max_buffer_len); 
  // move center to 0 and scale the values
  scaler.convert(buffer, len);

  int16_t *sample = (int16_t*) buffer; 
  int size = len / 4;
  for (int j=0; j<size; j++){
    Serial.print(*sample++);
    Serial.print(", ");
    Serial.println(*sample++);
  }
}