/**
 * @file streams-sam-webserver_wav.ino
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
#include "AudioTools.h"
#include "sam_arduino.h"

AudioWAVServer server("ssid","password");
int channels = 1;
int bits_per_sample = 16;

// Callback which provides the audio data 
void outputData(Print *out){
  Serial.print("providing data...");
  SAM sam(*out,  false);
  sam.say("hallo, I am SAM");
}

void setup(){
  Serial.begin(115200);
  // start data sink
  server.begin(outputData, SAM::sampleRate(), channels, bits_per_sample);
}

// Arduino loop  
void loop() {
  // Handle new connections
  server.copy();  
}