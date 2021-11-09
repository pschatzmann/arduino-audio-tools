/**
 * @file streams-ads1015-serial.ino
 * @author Phil Schatzmann
 * @brief Use ads1015 as input device
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include <Adafruit_ADS1X15.h>

using namespace audio_tools;  

const int sample_rate = 3000;
const int channels = 1;
const int bits_per_sample = 16;

Adafruit_ADS1015 ads1015; // ads1015 device  
TimerCallbackAudioStream in; 
CsvStream<int16_t> out(Serial);                        
StreamCopy copier(out, in); // copy i2sStream to a2dpStream

// Provides the data from the ADS1015
uint16_t IRAM_ATTR getADS1015(uint8_t *data, uint16_t len){
    int16_t sample = ads1015.readADC_Differential_0_1();
    memcpy(data,(void*) &sample, sizeof(int16_t));
    return sizeof(int16_t);
}


// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup gain for ads1015
  ads1015.setGain(GAIN_SIXTEEN);

  // Open stream from ads1015  
  auto cfg = in.defaultConfig();
  cfg.rx_tx_mode = RX_MODE;
  cfg.sample_rate = sample_rate;
  cfg.channels = channels;
  cfg.bits_per_sample = bits_per_sample;
  cfg.callback = getADS1015;
  in.setNotifyAudioChange(out);
  in.begin(cfg);
 
  // Output as CSV to serial 
  out.begin();

}

// Arduino loop - copy data 
void loop() {
  copier.copy();
}