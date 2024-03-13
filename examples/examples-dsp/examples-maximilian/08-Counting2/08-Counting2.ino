// This example shows how you can create a basic counter with a phasor.
// A phasor oscillator can create a ramp between any two values.
// It takes three inputs - frequency, start value and stop value.
// These are all double precision floats, so it's a continuous slide.
// If you write it into an integer, it will round it off for you.
// This creates a bunch of steps.

#include "AudioTools.h"
#include "AudioLibs/MaximilianDSP.h"

// Define Arduino output
I2SStream out;
Maximilian maximilian(out);

// Maximilian
maxiOsc myCounter,mySquare;//these oscillators will help us count and play sound
int CurrentCount;//we're going to put the current count in this variable so that we can use it more easily.


void setup() {//some inits
    // setup logging
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    // setup audio output
    auto cfg = out.defaultConfig(TX_MODE);
    out.begin(cfg);
    maximilian.begin(cfg);
}

void play(float *output) {
    
    // Here you can see that CurrentCount is an int. It's taking the continuous output of the phasor and convering it.
    // You don't need to explicityly 'cast' (i.e. change) the value from a float to an int.
    // It happens automagically in these cases.
    
    // Once every second, CurrentCount counts from 1 until it gets to 9, then resets itself.
    // When it reaches 9 it resets, so the values you get are 1-8.
    
    CurrentCount=myCounter.phasorBetween(1.0, 1.0, 9.0);//phasor can take three arguments; frequency, start value and end value.
    
    // If we multiply the output of CurrentCount by 100, we get 100,200,300,400,500,600,700,800 in that order.
    // These become the frequency of the oscillator.
    // In this case, the oscillator is an antialiased sawtooth wave. Yum.
    output[0]=mySquare.sawn(CurrentCount*100);
    output[1]=output[0];

}

// Arduino loop
void loop() {
    maximilian.copy();
}
