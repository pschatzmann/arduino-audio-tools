/**
 * @file streams-i2s-faust_guitarix-i2s.ino
 * @author Phil Schatzmann
 * @brief Example how to use Faust when Faust expects input and provides kitput
 * @version 0.1
 * @date 2022-04-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "AudioLibs/AudioBoardStream.h"
#include "AudioLibs/AudioFaust.h"
#include "guitarix.h"

AudioBoardStream kit(AudioKitEs8388V1); 
FaustStream<mydsp> faust(kit); // final output of Faust is kit
StreamCopy copier(faust, kit);  // copy data from kit to faust

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Setup Faust
  auto cfg = faust.defaultConfig();
  faust.begin(cfg);

  // Tube Screemer
  faust.setLabelValue("drive", 0.23);
  faust.setLabelValue("level", -12.6);
  faust.setLabelValue("tone", 489);
  // preamp
  faust.setLabelValue("Pregain", 0.4);
  faust.setLabelValue("Gain", 0.4);
  //jcm 2000
  faust.setLabelValue("Treble", 0.9);
  faust.setLabelValue("Middle", 0.76);
  faust.setLabelValue("Bass", 0.62);
  //Cabinet
  faust.setLabelValue("amount", 100);


  // start I2S
  auto cfg_i2s = kit.defaultConfig(RXTX_MODE);
  cfg_i2s.sample_rate = cfg.sample_rate; 
  cfg_i2s.channels = cfg.channels;
  cfg_i2s.bits_per_sample = cfg.bits_per_sample;
  cfg_i2s.input_device = ADC_INPUT_LINE1;
  kit.begin(cfg_i2s);

}

// Arduino loop - copy sound to kit 
void loop() {
  copier.copy();
}