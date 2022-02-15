// One way of thinking about FM synthesis is to see it as vibrato.
// You make a pitch, then vary it up and down at some rate.
// You can change the speed of the pitch variation (modulation frequency), and also the amount of variation (modulation index).
// In FM, usually only one of the waveforms - the carrier that provides the initial pitch - is sent to the output.
// The frequency of the the carrier wave is continually adjusted at a rate equal to the frequency of the second wave (the modulator).
// So at any given point in time, the frequency of the carrier can increase by an amount equal to the current amp of the modulator.
// This has some interesting effects.

#include "AudioTools.h"
#include "AudioLibs/MaximilianDSP.h"

// Define Arduino output
I2SStream out;
Maximilian maximilian(out);
// Maximilian
maxiOsc mySine,myOtherSine;//Two oscillators


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
    
    // In this example, the 'myOtherSine.sinewave' is at an amplitude of 1, it's original amplitude.
    // This is pretty simple and not too useful.
    //output[0]=mySine.sinewave(440*myOtherSine.sinewave(1));
    
    // Perhaps you should comment out the above line and uncomment the below one instead
    // It shows how the frequency of the carrier is altered by ADDING a second waveform to its frequency value.
    // The carrier frequency is 440, and the modulation frequency is 1.
    // It also shows how the modulation index works. In this case the modulation index is 100
    // Try adjusting the modolation index. Also, try altering the modulation frequency.
    output[0]=mySine.sinewave(440+(myOtherSine.sinewave(1)*100));
    output[1]=output[0];

}

// In complex FM systems you can have lots of modulators stacked together in interesting ways, and theoretically this can make any sound.
// John Chowning is the guy you probably want to talk to about that.

// Arduino loop
void loop() {
    maximilian.copy();
}