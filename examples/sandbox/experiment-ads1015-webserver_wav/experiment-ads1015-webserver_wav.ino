/**
 * @file streams-effect_adc1015-webserver_wav.ino
 * @author Phil Schatzmann
 * @brief Use ads1015 as input device
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "ADS1X15.h"



const int sample_rate = 1640;
const int channels = 1;
const int bits_per_sample = 16;

ADS1115 ads1015(0x48); // ads1015 device  
TimerCallbackAudioStream in; 
AnalogAudioStream out;                        
StreamCopy copier(out, in); // copy i2sStream to a2dpStream

// Provides the data from the ADS1015
uint16_t IRAM_ATTR getADS1015(uint8_t *data, uint16_t len){
    int16_t sample = ads1015.getValue();
    memcpy(data,(void*) &sample, sizeof(int16_t));
    return sizeof(int16_t);
}


// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // setup gain and start ads1015 
  ads1015.begin();
  if(!ads1015.isConnected()) LOGE("ads1015 NOT CONNECTED!");
  ads1015.setGain(4);        // 6.144 volt
  ads1015.setDataRate(7);    // 0 = slow   4 = medium   7 = fast  
  ads1015.setMode(0);
  ads1015.requestADC_Differential_0_1();


  // Output  
  auto cfgOut = out.defaultConfig();
  out.begin(cfgOut);


  // Open stream from ads1015  
  auto cfg = in.defaultConfig();
  cfg.rx_tx_mode = RX_MODE;
  cfg.sample_rate = sample_rate;
  cfg.channels = channels;
  cfg.bits_per_sample = bits_per_sample;
  cfg.callback = getADS1015;
  cfg.timer_function = SimpleThreadLoop;  //DirectTimerCallback, TimerCallbackInThread, SimpleThreadLoop 
  in.setNotifyAudioChange(out);
  in.begin(cfg);
 
}

// Arduino loop - copy data 
void loop() {
  copier.copy();
}
