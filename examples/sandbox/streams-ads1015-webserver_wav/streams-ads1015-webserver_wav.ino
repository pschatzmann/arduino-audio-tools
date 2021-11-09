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
#include "AudioExperiments.h"
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

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
const int sample_rate = 3000;
const int channels = 1;
const int bits_per_sample = 16;

// Audio 
Adafruit_ADS1015 ads1015; // ads1015 device  
TimerCallbackAudioStream ads1015Stream; // Input stream from ads1015
AudioEffects effects(ads1015Stream); // apply effects on ads1015Stream
GeneratedSoundStream<int16_t> in(effects); 


// Provides the data from the ADS1015
uint16_t getADS1015(uint8_t *data, uint16_t len){
    int16_t sample = ads1015.readADC_Differential_0_1();
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

  // setup gain 
  ads1015.setGain(GAIN_SIXTEEN);

  // configure & start cfg1015 Stream
  auto config = ads1015Stream.defaultConfig();
  config.rx_tx_mode = RX_MODE;
  config.channels = channels;
  config.sample_rate = sample_rate;
  config.bits_per_sample = bits_per_sample;
  config.secure_timer = true;
  config.callback = getADS1015;
  ads1015Stream.begin(config); // start ads1015

  // start server
  server.begin(in, config); 

  // start sound generator
  in.begin(config);

}

// copy the data
void loop() {
  server.doLoop();
}