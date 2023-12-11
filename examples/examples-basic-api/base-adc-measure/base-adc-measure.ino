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
  // Serial Interface
  Serial.begin(115200);
  // Include logging to serial
  AudioLogger::instance().begin(Serial, AudioLogger::Info); // Warning, INfo

  // Start ADC input
  Serial.println("Starting ADC...");
  auto adcConfig = analog_in.defaultConfig(RX_MODE);
  adcConfig.sample_rate = 2000000;
  // adcConfig.adc_bit_width = 12;  // platform & release dependent parameters
  analog_in.begin(adcConfig);

  // Start Measurements
  AudioLogger::instance().begin(Serial, AudioLogger::Warning); // Warning, INfo
  Serial.println("Starting through put measurement...");
}

void loop() { copier.copy(); }

/* Adafruit Feather ESP32-S3 2MB PSRAM
  Data: bytes per second, single unit
   SPS  :  expected,  measured (might be per channel)
    8,000:   64,000,    32,000
   11,025:   88,200,    44,000
   16,000:  128,000,    64,000
   20,000:  160,000,    80,000
   22,050:  176,400,    88,000
   44,100:  352,800,   179,000
   48,000:  384,000,   192,000
   88,200:    error, sample rate: 88200 error, range: 611 to 83333
*/

/* DOIT ESP32 DEVKIT V1
  Data: bytes per second, single unit
   SPS  :  expected,  measured (might be per channel) 80% of expected
   16000:  error, sample rate: 16000 warning, range: 20000 to 2000000
   20000:    80,000,    32,000
   22050:    88,200,    36,000
   44100:   176,400,    72,000
   48000:   192,000,    77,000
   88200:   352,800,   144,000
   96000:   384,000,   157,000
  176400:   704,000,   288,000
  192200:   768,800,   314,000
  352800: 1,411,200,   576,000
  384000: 1,536,000,   627,000
 1000000: 4,000,000, 1,640,000
 1500000: 6,000,000, 2,455,000
 2000000: 8,000,000, 3,271,000
*/
