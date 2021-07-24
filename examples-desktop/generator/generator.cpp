// Simple wrapper to make Arduino sketch compilable by cpp in cmake
#include "Arduino.h"
#include "generator.ino"

// Privide main()
int main(){
  setup();
  while(true){
    loop();
  }
}