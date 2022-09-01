/**
 * @file streams-audiokit-sd-audiokit.ino
 * @author Phil Schatzmann
 * @brief We record the input from the microphone to a file and constantly repeat to play the file
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
uint16_t sample_rate = 16000;
uint8_t channels = 1;  // The stream will have 2 channels 
AudioKitStream kit;
File file;   // final output stream
StreamCopy copier; // copies data
bool recording = false;  // flag to make sure that close is only executed one
uint64_t end_time; // trigger to call endRecord
 

void startRecord() {
  Serial.println("Recording...");
  // open the output file
  file = SD.open(file_name, FILE_WRITE);
  copier.begin(file, kit);  
  recording = true; 
}

void endRecord() {
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
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // Open SD drive
  if (!SD.begin(PIN_AUDIO_KIT_SD_CARD_CS)) {
    Serial.println("initialization failed!");
    while (1); // stop
  }
  Serial.println("initialization done.");

  // setup input and output
  auto cfg = kit.defaultConfig(RXTX_MODE);
  cfg.sd_active = true;
  cfg.sample_rate = sample_rate;
  cfg.channels = channels;
  cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
  kit.begin(cfg);
  kit.setVolume(1.0);

  startRecord();
  end_time = millis()+5000; // record for 5 seconds

}

void loop(){
  // time based stop recording
  if (recording && millis()>end_time){
    endRecord();
  }

  // record or play file
  copier.copy();  

  // while playing: at end of file -> reposition to beginning 
  if (file.size()>0 && file.available()==0){
      file.seek(0);
      Serial.println("Replay...");
  }
}