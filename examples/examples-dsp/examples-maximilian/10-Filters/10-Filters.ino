// Here is an example of a Maximilian filter being used.
// There are a number of filters in Maximilian, including low and high pass filters.
// There are also resonant filters and a state variable filter.


#include "AudioTools.h"
#include "AudioTools/AudioLibs/MaximilianDSP.h"

// Define Arduino output
I2SStream out;
Maximilian maximilian(out);

// Maximilian
maxiOsc myCounter,mySwitchableOsc;//
int CurrentCount;//
double myOscOutput,myCurrentVolume, myFilteredOutput;//
maxiEnv myEnvelope;
maxiFilter myFilter;

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
    
    myCurrentVolume=myEnvelope.adsr(1.,myEnvelope.trigger);
    
    CurrentCount=myCounter.phasorBetween(1.0, 1.0, 9.0);//phasor can take three arguments; frequency, start value and end value.
    
    // You'll notice that these 'if' statements don't require curly braces "{}".
    // This is because there is only one outcome if the statement is true.
    
    if (CurrentCount==1) myEnvelope.trigger=1; //trigger the envelope
    
    else myEnvelope.trigger=0;//release the envelope to make it fade out only if it's been triggered
    
    myOscOutput=mySwitchableOsc.sawn(100);
    
    // Below, the oscilator signals are being passed through a low pass filter.
    // The middle input is the filter cutoff. It is being controlled by the envelope.
    // Notice that the envelope is being amplified so that it scales between 0 and 1000.
    // The last input is the resonance.
    myFilteredOutput=myFilter.lores(myOscOutput,myCurrentVolume*1000,10);
    
    output[0]=myFilteredOutput;//left speaker
    output[1]=output[0];
    
}


// Arduino loop
void loop() {
    maximilian.copy();
}
