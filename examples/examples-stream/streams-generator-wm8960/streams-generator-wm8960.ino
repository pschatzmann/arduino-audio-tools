/**
 * @file streams-generator-wm8990.ino
 * @author Phil Schatzmann
 * @brief Test sketch for wm8990
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "AudioTools/AudioLibs/WM8960Stream.h"

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
WM8960Stream out; 
StreamCopy copier(out, sound);                             // copies sound into i2s

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  while(!Serial);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // setup wire on pins 19 and 21
  Wire.begin(19, 21);
  Wire.setClock(10000);


  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info); 
  config.wire = &Wire;
  // use default i2s pins 
  //config.pin_bck = 14;
  //config.pin_ws = 15;
  //config.pin_data = 22;

  if (!out.begin(config)){
    stop();
  }

  // Setup sine wave
  sineWave.begin(info, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
