
//This shows how to use maximilian to build a polyphonic synth.

#include "AudioTools.h"
#include "AudioTools/AudioLibs/MaximilianDSP.h"

// Define Arduino output
I2SStream out;
Maximilian maximilian(out);

//These are the synthesiser bits
maxiOsc VCO1[6],VCO2[6],LFO1[6],LFO2[6];
maxiFilter VCF[6];
maxiEnv V_ADSR[6];

//This is a bunch of control signals so that we can hear something
maxiOsc timer;//this is the metronome
int currentCount,lastCount,voice=0;//these values are used to check if we have a new beat this sample

//and these are some variables we can use to pass stuff around
double VCO1out[6],VCO2out[6],LFO1out[6],LFO2out[6],VCFout[6],ADSRout[6],mix,pitch[6];


void setup() {//some inits
    // setup logging
    Serial.begin(115200);
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

    // setup audio output
    auto cfg = out.defaultConfig(TX_MODE);
    out.begin(cfg);
    maximilian.begin(cfg);

    // setup maximilian       
    for (int i=0;i<6;i++) {
        V_ADSR[i].setAttack(0);
        V_ADSR[i].setDecay(200);
        V_ADSR[i].setSustain(0.2);
        V_ADSR[i].setRelease(2000);
    }
}

void play(float *output) {
    
    mix=0;//we're adding up the samples each update and it makes sense to clear them each time first.
    
    //so this first bit is just a basic metronome so we can hear what we're doing.
    
    currentCount=(int)timer.phasor(8);//this sets up a metronome that ticks 8 times a second
    
    if (lastCount!=currentCount) {//if we have a new timer int this sample, play the sound
        
        if (voice==6) {
            voice=0;
        }
        
        V_ADSR[voice].trigger=1;//trigger the envelope from the start
        pitch[voice]=voice+1;
        voice++;
        
    }
    
    //and this is where we build the synth
    
    for (int i=0; i<6; i++) {
        
        
        ADSRout[i]=V_ADSR[i].adsr(1.,V_ADSR[i].trigger);//our V_ADSR env is passed a constant signal of 1 to generate the transient.
        
        LFO1out[i]=LFO1[i].sinebuf(0.2);//this lfo is a sinewave at 0.2 hz
        
        VCO1out[i]=VCO1[i].pulse(55*pitch[i],0.6);//here's VCO1. it's a pulse wave at 55 hz, with a pulse width of 0.6
        VCO2out[i]=VCO2[i].pulse((110*pitch[i])+LFO1out[i],0.2);//here's VCO2. it's a pulse wave at 110hz with LFO modulation on the frequency, and width of 0.2
        
        
        VCFout[i]=VCF[i].lores((VCO1out[i]+VCO2out[i])*0.5, 250+((pitch[i]+LFO1out[i])*1000), 10);//now we stick the VCO's into the VCF, using the V_ADSR as the filter cutoff
        
        mix+=VCFout[i]*ADSRout[i]/6;//finally we add the V_ADSR as an amplitude modulator
        
        
    }
    
    output[0]=mix*0.5;//left channel
    output[1]=mix*0.5;//right channel
    
    
    // This just sends note-off messages.
    for (int i=0; i<6; i++) {
        V_ADSR[i].trigger=0;
    }
    
}


// Arduino loop
void loop() {
    maximilian.copy();
}
