/**
 * @file streams-audiokit-sd-audiokit.ino
 * @author Phil Schatzmann
 * @brief We record the input from the microphone to SPI RAM and constantly repeat to play the file
 * The input is triggered by pressing key 1. Recording stops when key 1 is released!
 * The output pitch is shifted by 1 octave!
 * @version 0.1
 * @date 2022-11-21
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioLibs/MemoryManager.h"

AudioInfo info(16000, 1, 16);
MemoryManager memory(500); // Activate SPI RAM for objects > 500 bytes
AudioBoardStream kit(AudioKitEs8388V1);
//use one of VariableSpeedRingBufferSimple, VariableSpeedRingBuffer, VariableSpeedRingBuffer180 
PitchShiftOutput<int16_t, VariableSpeedRingBuffer<int16_t>> pitch_shift(kit);
DynamicMemoryStream recording(false); // Audio stored on heap, non repeating
StreamCopy copier; // copies data
 

void record_start(bool pinStatus, int pin, void* ref){
  Serial.println("Recording...");
  recording.begin();
  copier.begin(recording, kit);  
}

void record_end(bool pinStatus, int pin, void* ref){
  Serial.println("Playing...");

  // Remove popping noise, from button: we delete 6 segments at the beginning and end 
  // and on the resulting audio we slowly raise the volume on the first segment
  // end decrease it on the last segment
  recording.postProcessSmoothTransition<int16_t>(info.channels, 0.01, 6);

  // output with pitch shifting
  copier.begin(pitch_shift, recording);  // start playback
}

void setup(){
  Serial.begin(115200);
  while(!Serial); // wait for serial to be ready
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  // setup input and output
  auto cfg = kit.defaultConfig(RXTX_MODE);
  cfg.sd_active = true;
  cfg.copyFrom(info);
  cfg.input_device = ADC_INPUT_LINE2;
  kit.begin(cfg);
  kit.setVolume(1.0);

  // setup pitch shift
  auto cfg_pc = pitch_shift.defaultConfig();
  cfg_pc.pitch_shift = 2; // one octave up
  cfg_pc.buffer_size = 1000;
  cfg_pc.copyFrom(info);
  pitch_shift.begin(cfg_pc);

  // record when key 1 is pressed
  kit.audioActions().add(kit.getKey(1), record_start, record_end);
  Serial.println("Press Key 1 to record");
}

void loop(){
  // record or play recording
  copier.copy();  

  // Process keys
  kit.processActions();
}
