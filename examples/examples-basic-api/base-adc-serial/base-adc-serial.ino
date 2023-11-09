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
  delay(3000); // Give time to boot
  Serial.begin(115200);
  // Include logging to serial
  AudioLogger::instance().begin(Serial, AudioLogger::Info); //Warning, Info
  Serial.println("starting ADC...");
  auto adcConfig = adc.defaultConfig(RX_MODE);
  adcConfig.sample_rate = 44100;
  // do not set bits_per_sample as that is configured based on adc_bit_with
  adcConfig.adc_bit_width = 12; // this will result in 16 bits_per_sample
  adc.begin(adcConfig);

  // delay(100);
  // if (adcConfig.rx_tx_mode == RX_MODE){
  //   Serial.println("Using RX Mode (ADC)");
  // }
  // else if (adcConfig.rx_tx_mode == TX_MODE) {
  //   Serial.println("Using TX Mode (DAC)");
  // }
  // Serial.printf("sample_rate is: %u\n", adcConfig.sample_rate);
  // Serial.printf("bits_per_sample is: %u\n", adcConfig.bits_per_sample);
  // Serial.printf("adc_attenuation is: %u\n", adcConfig.adc_attenuation);
  // Serial.printf("adc_bit_width is: %u\n", adcConfig.adc_bit_width);
  // Serial.printf("adc mode is: %u\n", adcConfig.adc_conversion_mode);
  // Serial.printf("adc format is: %u\n", adcConfig.adc_output_type);
  // for(int i=0; i<adcConfig.channels; i++) {
  //   Serial.printf("channel[%d] is on pin: %u\n", i, adcConfig.adc_channels[i]);
  // }
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