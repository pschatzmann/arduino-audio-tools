//This shows how to use maximilian to build a monophonic synth

#include "AudioTools.h"
#include "AudioLibs/MaximilianDSP.h"

// Define Arduino output
I2SStream out;
Maximilian maximilian(out);

//These are the synthesiser bits
maxiOsc VCO1,VCO2,LFO1,LFO2;
maxiFilter VCF;
maxiEnv V_ADSR;

//This is a bunch of control signals so that we can hear something
maxiOsc timer;//this is the metronome
int currentCount,lastCount;//these values are used to check if we have a new beat this sample

//and these are some variables we can use to pass stuff around

double VCO1out,VCO2out,LFO1out,LFO2out,VCFout,ADSRout;


void setup() {//some inits
    // setup logging
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    // setup audio output
    auto cfg = out.defaultConfig(TX_MODE);
    out.begin(cfg);
    maximilian.begin(cfg);

    // setup maximilian   
    
    V_ADSR.setAttack(1000);
    V_ADSR.setDecay(1);
    V_ADSR.setSustain(1);
    V_ADSR.setRelease(1000);
}

void play(float *output) {
    
    //so this first bit is just a basic metronome so we can hear what we're doing.
    
    currentCount=(int)timer.phasor(0.5);//this sets up a metronome that ticks every 2 seconds
    
    
    if (lastCount!=currentCount) {//if we have a new timer int this sample, play the sound
        V_ADSR.trigger=1;
        
        cout << "tick\n";//the clock ticks
        
        lastCount=0;//set lastCount to 0
    }
    
    //and this is where we build the synth
    
    ADSRout=V_ADSR.adsr(1.0,V_ADSR.trigger);
    
    LFO1out=LFO1.sinebuf(0.2);//this lfo is a sinewave at 0.2 hz
    
    VCO1out=VCO1.pulse(55,0.6);//here's VCO1. it's a pulse wave at 55 hz, with a pulse width of 0.6
    VCO2out=VCO2.pulse(110+LFO1out,0.2);//here's VCO2. it's a pulse wave at 110hz with LFO modulation on the frequency, and width of 0.2
    
    
    VCFout=VCF.lores((VCO1out+VCO2out)*0.5, ADSRout*10000, 10);//now we stick the VCO's into the VCF, using the V_ADSR as the filter cutoff
    
    double finalSound=VCFout*ADSRout;//finally we add the V_ADSR as an amplitude modulator
    V_ADSR.trigger=0;
    output[0]=finalSound;
    output[1]=finalSound;
}

// Arduino loop
void loop() {
    maximilian.copy();
}
