/**
 * @file streams-kit-filter-kit.ino
 * @brief Copy audio from I2S to I2S using an FIR filter
 * @author Phil Schatzmann
 * @copyright GPLv3
 * #TODO testing is outstanding
 */

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
 
uint16_t sample_rate=44100;
uint16_t channels = 2;
AudioKitStream kit;

// copy filtered values
FilteredStream<int16_t, float> inFiltered(kit, channels);  // Defiles the filter as BaseConverter
StreamCopy copier(kit, inFiltered);               // copies sound into i2s

// define FIR filter
float coef[] = { 0.021, 0.096, 0.146, 0.096, 0.021};

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  // change to Warning to improve the quality
  AudioLogger::instance().begin(Serial, AudioLogger::Info); 

  // setup filters for all available channels
  inFiltered.setFilter(0, new FIR<float>(coef));
  inFiltered.setFilter(1, new FIR<float>(coef));

  // start I2S in
  Serial.println("starting KIT...");
  auto config = kit.defaultConfig(RX_TX_MODE);
  config.sample_rate = sample_rate; 
  config.sd_active = false;
  config.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
  kit.begin(config);

  Serial.println("KIT started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
