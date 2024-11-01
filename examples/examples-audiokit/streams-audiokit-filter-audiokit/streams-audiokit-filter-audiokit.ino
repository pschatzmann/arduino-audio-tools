/**
 * @file streams-kit-filter-kit.ino
 * @brief Copy audio from I2S to I2S using an FIR filter
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
 
AudioInfo info(44100, 2, 16);
AudioBoardStream kit(AudioKitEs8388V1);

// copy filtered values
FilteredStream<int16_t, float> filtered(kit, info.channels);  // Defiles the filter as BaseConverter
StreamCopy copier(filtered, kit); // copies sound into i2s (both from kit to filtered or filered to kit are supported)

// define FIR filter
float coef[] = { 0.021, 0.096, 0.146, 0.096, 0.021};

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  // change to Warning to improve the quality
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info); 

  // setup filters for all available channels
  filtered.setFilter(0, new FIR<float>(coef));
  filtered.setFilter(1, new FIR<float>(coef));

  // start I2S in
  Serial.println("starting KIT...");
  auto config = kit.defaultConfig(RXTX_MODE);
  config.copyFrom(info); 
  config.sd_active = false;
  config.input_device = ADC_INPUT_LINE2;
  kit.begin(config);

  Serial.println("KIT started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}