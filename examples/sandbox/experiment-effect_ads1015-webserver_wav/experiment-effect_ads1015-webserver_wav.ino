/**
 * @file streams-ads1015-server_wav.ino
 *
 *  This sketch uses sound effects applied to a sine wav. The result is provided as WAV stream which can be listened to in a Web Browser
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#include "AudioTools.h"
#include "ADS1X15.h"

using namespace audio_tools;

// WIFI
const char *ssid = "ssid";
const char *password = "password";
AudioWAVServer server(ssid, password);

// Effects control input
float volumeControl = 1.0;
int16_t clipThreashold = 4990;
float fuzzEffectValue = 6.5;
int16_t distortionControl = 4990;
int16_t tremoloDuration = 200;
float tremoloDepth = 0.5;

// Audio Format
const int sample_rate = 1640;
const int channels = 1;
const int bits_per_sample = 16;

// Audio 
ADS1115 ads1015(0x48); // ads1015 device  
TimerCallbackAudioStream ads1015Stream; // Input stream from ads1015
AudioEffects effects(ads1015Stream); // apply effects on ads1015Stream
GeneratedSoundStream<int16_t> in(effects); 


// Provides the data from the ADS1015
uint16_t getADS1015(uint8_t *data, uint16_t len){
    int16_t sample = ads1015.getValue();
    memcpy(data,(void*) &sample, sizeof(int16_t));
    return sizeof(int16_t);
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Info);

  // setup effects
  effects.addEffect(new Boost(volumeControl));
  effects.addEffect(new Distortion(clipThreashold));
  effects.addEffect(new Fuzz(fuzzEffectValue));
  effects.addEffect(new Tremolo(tremoloDuration, tremoloDepth, sample_rate));

  // setup gain for ads1015
  ads1015.begin();
  if(!ads1015.isConnected()) LOGE("ads1015 NOT CONNECTED!");
  ads1015.setGain(4);        // 6.144 volt
  ads1015.setDataRate(7);    // 0 = slow   4 = medium   7 = fast  
  ads1015.setMode(0);
  ads1015.requestADC_Differential_0_1();
  // cfgure & start cfg1015 Stream
  auto cfg = ads1015Stream.defaultConfig();
  cfg.rx_tx_mode = RX_MODE;
  cfg.channels = channels;
  cfg.sample_rate = sample_rate;
  cfg.bits_per_sample = bits_per_sample;
  cfg.timer_function = SimpleThreadLoop;  //DirectTimerCallback, TimerCallbackInThread, SimpleThreadLoop 
  cfg.callback = getADS1015;
  ads1015Stream.begin(cfg); // start ads1015

  // start server
  server.begin(in, cfg); 

  // start sound generator
  in.begin(cfg);

}

// copy the data
void loop() {
  server.doLoop();
}