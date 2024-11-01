/**
 * @file read-csv.ino
 * @author Phil Schatzmann
 * @brief Test case for the new continuous API of the ESP32:
 * Default Pins ESP32: 34, 35 (ADC_CHANNEL_6, ADC_CHANNEL_7)
 * Default Pins ESP32C3: 2, 3 (ADC_CHANNEL_2, ADC_CHANNEL_3)
 * @version 0.1
 * @date 2023-11-01
 *
 * @copyright Copyright (c) 2023
 */
#include "AudioTools.h"

typedef uint16_t audio_t;
AudioInfo info(44100, 1, 8 * sizeof(audio_t));
// input
AnalogAudioStream in;
CsvOutput<audio_t> csv(Serial); // ASCII output stream
StreamCopy copy_in(csv, in);

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);


  auto cfg_rx = in.defaultConfig(RX_MODE);
  //cfg_rx.is_auto_center_read = false;
  //cfg_rx.adc_calibration_active = true;
  cfg_rx.copyFrom(info);
  in.begin(cfg_rx);

  // open output
  csv.begin(info);

  Serial.println("started...");
}

// Arduino loop - copy sound to out
void loop() {
  copy_in.copy();
}