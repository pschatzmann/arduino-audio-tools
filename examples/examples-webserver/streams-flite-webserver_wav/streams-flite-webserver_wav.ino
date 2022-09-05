/**
 * @file streams-flite-webserver_wav.ino
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#include "flite_arduino.h"
#include "AudioTools.h"

AudioWAVServer server("ssid","password");

// Callback which provides the audio data 
void outputData(Print *out){
  Serial.print("providing data...");
  Flite flite(*out);

  // Setup Audio Info
  FliteOutputBase *o = flite.getOutput();
  
  flite.say("Hallo, my name is Alice");
  Serial.printf("info %d, %d, %d", o->sampleRate(), o->channels(), o->bitsPerSample());
}

void setup(){
  Serial.begin(115200);
  server.begin(outputData, 8000,1,16);
}


// Arduino loop  
void loop() {
  // Handle new connections
  server.copy();  
}
