/**
 * @file streams-audiokit-sd-audiokit.ino
 * @author Phil Schatzmann
 * @brief We record the input from the microphone to a file and constantly repeat to play the file
 * The input is triggered by pressing key 1. Recording stops when key 1 is released!
 * @version 0.1
 * @date 2022-09-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/MemoryManager.h"

uint16_t sample_rate = 16000;
uint8_t channels = 1;  // The stream will have 2 channels 
AudioKitStream kit;
MemoryManager memory(500); // Activate PSRAM for objects > 500 bytes
DynamicMemoryStream recording(true); // Audio Stored on heap (PSRAM)
StreamCopy copier; // copies data
bool is_recording = false;  // flag to make sure that close is only executed one
uint64_t end_time; // trigger to call endRecord
 

void record_start(bool pinStatus, int pin, void* ref){
  Serial.println("Recording...");
  recording.begin();
  copier.begin(recording, kit);  
  is_recording = true; 
}

void record_end(bool pinStatus, int pin, void* ref){
  if (recording == true){
    Serial.println("Playing...");
    is_recording = false;
    copier.begin(kit, recording);  // start playback
  }
}

void setup(){
  Serial.begin(115200);
  while(!Serial); // wait for serial to be ready
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // setup input and output
  auto cfg = kit.defaultConfig(RXTX_MODE);
  cfg.sd_active = true;
  cfg.sample_rate = sample_rate;
  cfg.channels = channels;
  cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
  kit.begin(cfg);
  kit.setVolume(1.0);

  // record when key 1 is pressed
  kit.audioActions().add(PIN_KEY1, record_start, record_end);
  Serial.println("Press Key 1 to record");

}

void loop(){

  // record or play recording
  copier.copy();  

  // Process keys
  kit.processActions();

}
