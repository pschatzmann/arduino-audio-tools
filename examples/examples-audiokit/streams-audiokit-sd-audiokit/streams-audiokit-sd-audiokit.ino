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
#include <SPI.h>
#include <SD.h>

const char *file_name = "/rec.raw";
AudioInfo info(16000, 1, 16);
AudioKitStream kit;
File file;   // final output stream
StreamCopy copier; // copies data
bool recording = false;  // flag to make sure that close is only executed one
uint64_t end_time; // trigger to call endRecord
 

void record_start(bool pinStatus, int pin, void* ref){
  Serial.println("Recording...");
  // open the output file
  file = SD.open(file_name, FILE_WRITE);
  copier.begin(file, kit);  
  recording = true; 
}

void record_end(bool pinStatus, int pin, void* ref){
  if (recording == true){
    Serial.println("Playing...");
    file.close();
    recording = false;
    file = SD.open(file_name); // reopen in read mode
    copier.begin(kit, file);  // start playback
  }
}

void setup(){
  Serial.begin(115200);
  while(!Serial); // wait for serial to be ready
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);  

  // setup input and output: setup audiokit before SD!
  auto cfg = kit.defaultConfig(RXTX_MODE);
  cfg.sd_active = true;
  cfg.copyFrom(info);
  cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
  kit.begin(cfg);
  kit.setVolume(1.0);

  // Open SD drive
  if (!SD.begin(PIN_AUDIO_KIT_SD_CARD_CS)) {
    Serial.println("Initialization failed!");
    while (1); // stop
  }
  Serial.println("Initialization done.");


  // record when key 1 is pressed
  kit.audioActions().add(PIN_KEY1, record_start, record_end);
  Serial.println("Press Key 1 to record");

}

void loop(){

  // record or play file
  copier.copy();  

  // while playing: at end of file -> reposition to beginning 
  if (!recording && file.size()>0 && file.available()==0){
      file.seek(0);
      Serial.println("Replay...");
  }

  // Process keys
  kit.processActions();

}