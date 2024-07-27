/**
 * @file base-adc-average-mono-serial.ino
 * @brief Attempts down sampling with binning of a mono audio signal by AVG_LEN
 * @author Urs Utzinger
 * @copyright GPLv3
 **/

#include "Arduino.h"
#include "AudioTools.h"

// On board analog to digital converter
AnalogAudioStream analog_in; 

// Serial terminal output
CsvOutput<int16_t> serial_out(Serial);

#define BAUD_RATE   500000
#define SAMPLE_RATE 44100
#define BUFFER_SIZE 256
#define AVG_LEN 64
#define BYTES_PER_SAMPLE sizeof(int16_t)

// buffer to read samples and store the average samples
int16_t buffer[BUFFER_SIZE];

void setup() {
  
  delay(3000); // wait for serial to become available

  // Serial Interface
  Serial.begin(BAUD_RATE);

  // Include logging to serial
  AudioLogger::instance().begin(Serial, AudioLogger::Error); // Debug, Warning, Info, Error
  
  // Start ADC input
  Serial.println("Starting ADC...");
  auto adcConfig = analog_in.defaultConfig(RX_MODE);
  adcConfig.sample_rate = SAMPLE_RATE;
  adcConfig.channels = 1; 

  // For ESP32 by Espressif Systems version 3.0.0 and later:
  // see examples/README_ESP32.md
  // adcConfig.sample_rate = 44100;
  // adcConfig.adc_bit_width = 12;
  // adcConfig.adc_calibration_active = true;
  // adcConfig.is_auto_center_read = false;
  // adcConfig.adc_attenuation = ADC_ATTEN_DB_12; 
  // adcConfig.channels = 1;
  // adcConfig.adc_channels[0] = ADC_CHANNEL_4; 

  analog_in.begin(adcConfig);

  // Start Serial Output CSV
  Serial.println("Starting Serial Out...");
  auto csvConfig = serial_out.defaultConfig();
  csvConfig.sample_rate = SAMPLE_RATE/AVG_LEN;
  csvConfig.channels = 1;
  csvConfig.bits_per_sample = 16;
  serial_out.begin(csvConfig);
}


void loop() {

  // Read the values from the ADC buffer to local buffer
  size_t bytes_read   = analog_in.readBytes((uint8_t*) buffer, BUFFER_SIZE * BYTES_PER_SAMPLE); // read byte stream and  cast to destination type
  size_t samples_read = bytes_read/BYTES_PER_SAMPLE; 
  size_t avg_samples  = samples_read/AVG_LEN;

  // Average the samples over AVG_LEN
  int32_t sum; // register to hold summed values
  int16_t *sample = (int16_t*) buffer; // sample pointer (input)
  int16_t *avg = (int16_t*) buffer; // result pointer (output)
  // each time we access a sample we increment sample pointer
  for(uint16_t j=0; j<avg_samples; j++){
    sum = *sample++; // initialize sum
    for (uint16_t i=1; i<AVG_LEN; i++){
      sum += *sample++; // sum the samples
    }
    // compute average and store in output buffer
    *avg++ = (int16_t) (sum/AVG_LEN); 
  }

  serial_out.write((uint8_t*)buffer, avg_samples*BYTES_PER_SAMPLE); // stream to output as byte type
}
