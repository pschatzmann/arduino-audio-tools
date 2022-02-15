#include "AudioTools.h"
#include "AudioLibs/MaximilianDSP.h"

// Define Arduino output
I2SStream out;
Maximilian maximilian(out);

//This shows how to use maximilian to do basic amplitude modulation.
//It also shows what happens when you modulate waves with waves that have frequencies over 20 hz.
//You start to get interesting effects.

maxiOsc mySine,myOtherSine,myPhasor;//Three oscillators. They can be called anything. They can be any of the available waveforms.


void setup() {//some inits
  // setup logging
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup Aduio output
  auto cfg = out.defaultConfig(TX_MODE);
  out.begin(cfg);
  maximilian.begin(cfg);
}

void play(float *output) {
    
    //Using the phasor we can create a ramp, and use this ramp to set the frequency of one of the waves.
    //When the frequency of the lower waveform passes over the threshold of 20hz, we start to hear two new waveforms.
    //The frequency of the first new wave is the sum of the two original waves.
    //The frequency of the second new wave is the difference of the two original waves.
    //So you hear two new waves, one going up, one going down.
    
    output[0]=mySine.sinewave(440)*myOtherSine.sinewave(myPhasor.phasorBetween(0.01,0,440));
    output[1]=output[0];
   
}

// Arduino loop
void loop() {
    maximilian.copy();
}