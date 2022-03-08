/**
 * @file file_mp3-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/file_mp3-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// Add this in your sketch or change the setting in AudioConfig.h
#define USE_A2DP

#include <SPI.h>
#include <SD.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "BluetoothA2DPSource.h"
#include "AudioTools.h"



const int sd_ss_pin = 5;
const char* fileName = "/audio.mp3";
BluetoothA2DPSource a2dp_source;
AudioFileSourceSD *file=nullptr;
AudioGeneratorMP3 *mp3=nullptr;
AudioOutputWithCallback *out=nullptr;
volatile float volume = 1.0;  // you can update this value to change the volue


// callback used by A2DP to provide the sound data
int32_t get_sound_data(Frame* data, int32_t len) {  
  // get data
  if (out == nullptr) return 0;
  int32_t result = out->read(data, len);
  // set volume
  for (int j=0;j<len;j++){
    data[j].channel1 = data[j].channel1 * volume;
    data[j].channel2 = data[j].channel2 * volume;
  }
  return result;
}


// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  audioLogger = &Serial;

  // Setup Audio
  file = new AudioFileSourceSD(); 
  mp3 = new AudioGeneratorMP3();
  out = new AudioOutputWithCallback(512,5);
  
  // Open MP3 file and play it
  SD.begin(sd_ss_pin);
  if (file->open(fileName)) {
    
    // start the bluetooth
    Serial.println("starting A2DP...");
    a2dp_source.start("MyMusic", get_sound_data);  

    // start to play the file
    mp3->begin(file, out);
    Serial.printf("Playback of '%s' begins...\n", fileName);
  } else {
    Serial.println("Can't find .mp3 file");
  }
}

// Arduino loop - repeated processing 
void loop() {
  if (mp3->isRunning()) {
    if (!mp3->loop()) mp3->stop();
  } else {
    Serial.println("MP3 done");
    delay(10000);
  }
}
