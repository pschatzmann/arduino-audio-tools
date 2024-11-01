/**
 * @file streams-generator-vs1053
 * @author Phil Schatzmann
 * @brief Generate Test Tone
 * @version 0.1
 * @date 2022-08-10
 * 
 * @copyright Copyright (c) 2022
 * 
 */

// install https://github.com/pschatzmann/arduino-vs1053.git

#include "AudioTools.h"
#include "AudioTools/AudioLibs/VS1053Stream.h"

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
VS1053Stream out;    // VS1053 output
StreamCopy copier(out, sound); // copy sound to decoder


void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning); // Info is causing a lot of noise  

  // Setup sine wave
  sineWave.begin(info, N_A4);

  // setup output
  auto cfg = out.defaultConfig();
  cfg.copyFrom(info);
  // Use your custom pins or define in AudioCodnfig.h
  //cfg.cs_pin = VS1053_CS; 
  //cfg.dcs_pin = VS1053_DCS;
  //cfg.dreq_pin = VS1053_DREQ;
  //cfg.reset_pin = VS1053_RESET;
  if (!out.begin(cfg)){
      Serial.println("SPI Error");
      stop();
  }

}

void loop(){
  copier.copy();
}
