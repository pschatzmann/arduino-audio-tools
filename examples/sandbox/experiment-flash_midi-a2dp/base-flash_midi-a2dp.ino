/**
 * @file flash_midi-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/flash_midi-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// Add this in your sketch or change the setting in AudioConfig.h
#define USE_A2DP

#include <AudioFileSourcePROGMEM.h>
#include "AudioGeneratorMIDI.h"
#include "ESP8266AudioSupport.h"
#include "BluetoothA2DPSource.h"
#include "AudioTools.h"
#include "Undertale_Megalovania.h" // midi
#include "mgm1.h" // 1mgm_sf2 soundfont file



const int sd_ss_pin = 5;
BluetoothA2DPSource a2dp_source;
AudioFileSourcePROGMEM *file;
AudioFileSourcePROGMEM *sf;
AudioGeneratorMIDI *midi;
AudioOutputWithCallback *out;

// callback used by A2DP to provide the sound data
int32_t get_sound_data(Frame* data, int32_t len) {
  return out == nullptr ? 0 : out->read(data, len);
}


// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  audioLogger = &Serial;

  // Setup Audio
  sf = new AudioFileSourcePROGMEM(mgm1_sf2, mgm1_sf2_len); 
  file = new AudioFileSourcePROGMEM(Undertale_Megalovania_midi, Undertale_Megalovania_midi_len); 
  midi = new AudioGeneratorMIDI();
  out = new AudioOutputWithCallback(128*5,10);
  midi->SetSoundfont(sf);
  midi->SetSampleRate(44100);
  
  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dp_source.start("MyMusic", get_sound_data);  

  // start to play the file
  midi->begin(file, out);
  Serial.println("Playback of midi begins...");
}

// Arduino loop - repeated processing 
void loop() {
  if (midi->isRunning()) {
    if (!midi->loop()) midi->stop();
  } else {
    Serial.println("MP3 done");
    delay(10000);
  }
}