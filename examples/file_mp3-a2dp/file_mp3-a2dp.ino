#include <SPI.h>
#include <SD.h>
#include "AudioFileSourceSPIFFS.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputWithCallback.h"
#include "BluetoothA2DPSource.h"
#include "SoundTools.h"

using namespace sound_tools;  

const int sd_ss_pin = 5;
String fileName = "";
BluetoothA2DPSource a2dp_source;
AudioFileSourceSPIFFS *file;
AudioFileSourceID3 *id3;
AudioGeneratorMP3 *mp3;
AudioOutputWithCallback *out;

// callback used by A2DP to provide the sound data
int32_t get_sound_data(Channels* data, int32_t len) {
  return out.read(data, len);
}

// finds a mp3 file
bool findMp3File() {
  File dir, root = SPIFFS.open("/");  
  while ((dir = root.openNextFile())) {
    if (String(dir.name()).endsWith(".mp3")) {
      if (file->open(dir.name())) {
        fileName = String(dir.name());
        break;
      }
    }
    dir = root.openNextFile();
  }
  return fileName.length() > 0;
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  audioLogger = &Serial;

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dp_source.start("MyMusic", get_sound_data);  

  // Setup Audio
  SPIFFS.begin(sd_ss_pin);
  file = new AudioFileSourceSPIFFS(); 
  mp3 = new AudioGeneratorMP3();
  out = new AudioOutputWithCallback(1024,5);
  
  // Find first MP3 file in SPIFF and play it
  if (findMp3File()) {
    id3 = new AudioFileSourceID3(file);
    mp3->begin(id3, out);
    Serial.printf("Playback of '%s' begins...\n", fileName.c_str());
  } else {
    Serial.println("Can't find .mp3 file in SPIFFS");
  }
}

// Arduino loop - repeated processing 
void loop() {
  if (mp3->isRunning()) {
    if (!mp3->loop()) mp3->stop();
  } else {
    Serial.println("MP3 done");
    delay(1000);
  }
}