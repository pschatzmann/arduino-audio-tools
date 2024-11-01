//Envelopes allow you to shape the sound. The basic idea is that a sound has the following shape
// Attack: This is how long it takes to fade up to maximum volume
// Decay: This is how long it takes to reach the sustain level.
// Sustain: This is the sustain level
// Release: This is how long it takes to fade out.

#include "AudioTools.h"
#include "AudioTools/AudioLibs/MaximilianDSP.h"

// Define Arduino output
I2SStream out;
Maximilian maximilian(out);

// Maximilian
maxiOsc myCounter,mySwitchableOsc;//
int CurrentCount;//
double myOscOutput,myCurrentVolume;//
maxiEnv myEnvelope;


void setup() {//some inits
    // setup logging
    Serial.begin(115200);
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

    // setup audio output
    auto cfg = out.defaultConfig(TX_MODE);
    out.begin(cfg);
    maximilian.begin(cfg);

    
    //Timing is in ms    
    myEnvelope.setAttack(0);
    myEnvelope.setDecay(1);  // Needs to be at least 1
    myEnvelope.setSustain(1);
    myEnvelope.setRelease(1000);
    
}

void play(float *output) {
    
    //notice that we feed in a value of 1. to create an envelope shape we can apply later.
    myCurrentVolume=myEnvelope.adsr(1.,myEnvelope.trigger);
    
    CurrentCount=myCounter.phasorBetween(1, 1, 9);//phasor can take three arguments; frequency, start value and end value.
    
    // You'll notice that these 'if' statements don't require curly braces "{}".
    // This is because there is only one outcome if the statement is true.
    
    if (CurrentCount==1) myEnvelope.trigger=1; //trigger the envelope
    
    else myEnvelope.trigger=0;//release the envelope to make it fade out only if it's been triggered
    
    if (CurrentCount<5)
        
        myOscOutput=mySwitchableOsc.sawn(CurrentCount*100);
    
    else if (CurrentCount>=5)//and the 'else' bit.
        
        myOscOutput=mySwitchableOsc.sinewave(CurrentCount*50);//one osc object can produce whichever waveform you want.
    
    
    output[0]=myOscOutput*myCurrentVolume;//left speaker
    output[1]=output[0];
    
}

// Arduino loop
void loop() {
    maximilian.copy();
}
