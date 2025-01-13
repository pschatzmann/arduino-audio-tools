/**
 * @file streams-url_flac_foxen-i2s.ino
 * @author Phil Schatzmann
 * @brief Demo using the Foxen FLAC decoder 
 * @version 0.1
 * @date 2025-01-09
 * 
 * @copyright Copyright (c) 2022
 * 
 * Warning: The WIFI speed is quite limited and the FLAC files are quite big. So there
 * is a big chance that the WIFI is just not fast enough. For my test I was downsampling
 * the file to 8000 samples per second! 
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecFLACFoxen.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

const char* ssid = "ssid";
const char* pwd = "password";
URLStream url(ssid, pwd);
AudioBoardStream i2s(AudioKitEs8388V1); // or replace with e.g. I2SStream i2s;

FLACDecoderFoxen flac(5*1024, 2);
EncodedAudioStream dec(&i2s, &flac); // Decoding to i2s
StreamCopy copier(dec, url, 1024); // copy url to decoder

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);  
  while(!Serial);
  Serial.println("starting...");
 
  // Open I2S: if the buffer is too slow audio is breaking up
  auto cfg =i2s.defaultConfig(TX_MODE); 
  cfg.buffer_size= 1024;
  cfg.buffer_count = 20;
  if (!i2s.begin(cfg)){
    Serial.println("i2s error");
    stop();
  }

  // Open url
  url.begin("https://pschatzmann.github.io/Resources/audio/audio.flac", "audio/flac");

  // Start decoder
  if (!dec.begin()){
    Serial.println("Decoder failed");
  }
}

void loop() {
  copier.copy();
}