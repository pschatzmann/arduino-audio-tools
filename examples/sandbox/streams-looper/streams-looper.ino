/**
 * @file streams-looper.ino
 * @author Phil Schatzmann (you@domain.com)
 * @brief A simple Looping Device. When we press key 1 we record (and mix) a new track
 * @version 0.1
 * @date 2022-08-05
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/MemoryManager.h"

AudioActions actions; // Handle Keys
MemoryManager memory(500); // Activate PSRAM
DynamicMemoryStream audio_played(true); // Audio Stored on heap (PSRAM)
DynamicMemoryStream audio_recorded(true); // Audio Stored on heap (PSRAM)
AudioKitStream kit; // Access I2S as stream
InputMixer<int16_t> mixer; // mix microphone with 
StreamCopy copier_in(audio_recorded, mixer);  // mix audio with recorded audio
StreamCopy copier_out(kit, audio_played);  // output mixed sound

int voices = 0;
bool is_play = false;
bool is_record = false;

// Starts the recoring of a new track
void record_start(bool pinStatus, int pin, void* ref){
  Serial.println("record_start");
  voices++;
  is_record = true; 
  audio_recorded.begin();
  if (voices==1){
    audio_played.begin();  // starts DynamicMemoryStream
  } 
}

// Plays the recorded audio
void record_end(bool pinStatus, int pin, void* ref){
  Serial.println("record_end");
  audio_played.clear(); 
  audio_played.assign(audio_recorded); // move audio_new to audio
  audio_recorded.clear();
  is_play = true;
  is_record = false;
}


void setup() {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Warning);

    mixer.add(kit);
    mixer.add(audio_played);
    
    // configure & start AudioKit
    auto cfg = kit.defaultConfig(RXTX_MODE);
    cfg.sd_active = false;
    cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
    kit.begin(cfg);

    // configure & start mixer
    mixer.begin(cfg);

    // record when key 1 is pressed
    actions.add(PIN_KEY1, record_start, record_end);
}

void loop() {
  if (is_record){
    copier_in.copy(); // copy mixed input with recorded audio
  }
  if (is_play){
    copier_out.copy();
  }
}