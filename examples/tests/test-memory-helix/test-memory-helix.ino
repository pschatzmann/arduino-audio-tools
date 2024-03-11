
#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"

/**
 * @brief Sketch to test the memory usage with libhelix with an ESP32
 * => Available heap 140980-151484
 */



URLStream url("SSID","password");
I2SStream i2s; // final output of decoded stream
VolumeStream volume(i2s);
LogarithmicVolumeControl lvc(0.1);
EncodedAudioStream dec(&volume,new MP3DecoderHelix()); // Decoding stream
StreamCopy copier(dec, url); // copy url to decoder

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  i2s.begin(config);

  // setup I2S based on sampling rate provided by decoder
  dec.begin();

// set initial volume
  volume.setVolumeControl(lvc);
  volume.setVolume(0.5);

// mp3 radio
  url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128","audio/mp3");
}

uint32_t i=0;
uint32_t minFree=0xFFFFFFFF;
uint32_t maxFree=0;
uint32_t actFree;

void minMax(){
  if (i%1000==0){
    actFree = ESP.getFreeHeap();
    if (actFree < minFree){
      minFree = actFree;
    }
    if (actFree > maxFree){
      maxFree = actFree;
    }
    Serial.println();
    Serial.print(actFree);
    Serial.print(":");
    Serial.print(minFree);
    Serial.print("-");
    Serial.println(maxFree);
  }
  
}

void loop(){
  copier.copy();
  minMax();
}
