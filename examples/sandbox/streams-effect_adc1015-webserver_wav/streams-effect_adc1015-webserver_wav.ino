/**
 * @file streams-effect_adc1015-server_wav.ino
 *
 * This sketch uses sound effects applied to the guitar sound provided by the adc1015. 
 * The result is provided as WAV stream which can be listened to in a Web Browser
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#include "AudioTools.h"
#include "AudioExperiments.h"
#include <Adafruit_ADS1X15.h>

using namespace audio_tools;

// WIFI
const char *ssid = "ssid";
const char *password = "password";
AudioWAVServer server(ssid, password);

// Contorl input
float volumeControl = 1.0;
int16_t clipThreashold = 4990;
float fuzzEffectValue = 6.5;
int16_t distortionControl = 4990;
int16_t tremoloDuration = 200;
float tremoloDepth = 0.5;

// Audio ads1015 -> AudioEffects -> AudioEffects -> GeneratedSoundStream
Adafruit_ADS1015 ads1015; // ads1015 device  
TimerCallbackAudioStream in; 
AudioEffects effects(in);
GeneratedSoundStream<int16_t> gen(effects); 

// Audio Format
const int sample_rate = 3000;
const int channels = 1;
const int bits_per_sample = 16;


// Provides the data from the ADS1015
uint16_t IRAM_ATTR getADS1015(uint8_t *data, uint16_t len){
    int16_t sample = ads1015.readADC_Differential_0_1();
    memcpy(data,(void*) &sample, sizeof(int16_t));
    return sizeof(int16_t);
}


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Info);

  // setup gain and start ads1015 
  ads1015.setGain(GAIN_SIXTEEN);
  ads1015.begin();

  // setup effects
  effects.addEffect(new Boost(volumeControl));
  effects.addEffect(new Distortion(clipThreashold));
  effects.addEffect(new Fuzz(fuzzEffectValue));
  effects.addEffect(new Tremolo(tremoloDuration, tremoloDepth, sample_rate));

  // setup input
  auto cfg = in.defaultConfig();
  cfg.rx_tx_mode = RX_MODE;
  cfg.sample_rate = sample_rate;
  cfg.channels = channels;
  cfg.bits_per_sample = bits_per_sample;
  cfg.callback = getADS1015;
  cfg.secure_timer = true;
  in.begin(cfg);

  // start server
  server.begin(gen, cfg);

}

// copy the data
void loop() {
  server.doLoop();
}