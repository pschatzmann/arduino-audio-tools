#include "maximilian.h"

//this tutorial explains how to use the maxiEnv

maxiSample sound1;

maxiOsc timer,snarePhase; //and a timer

maxiEnv envelope;//this is going to be an envelope

int currentCount,lastCount,playHead,

sequence[16]={1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0}; //This is the sequence for the kick

int sampleTrigger;

double sampleOut;

void setup() {//some inits
	
    //YOU HAVE TO PROVIDE THE SAMPLES....
    
	sound1.load("/Users/mickgrierson/Documents/audio/68373__juskiddink__Cello_open_string_bowed.wav");//load in your samples. Provide the full path to a wav file.
	
	
	printf("Summary:\n%s", sound1.getSummary());//get info on samples if you like.
	//beats.getLength();
}

void play(double *output) {//this is where the magic happens. Very slow magic.
	
	currentCount=(int)timer.phasor(8);//this sets up a metronome that ticks 8 times a second
	
	
	if (lastCount!=currentCount) {//if we have a new timer int this sample, play the sound
		
		sampleTrigger=sequence[playHead%16];
		playHead++;//iterate the playhead
		lastCount=0;//reset the metrotest
	
	}
	
	//the envelope we're using here is an AR envelope.
	//It has an input (which in this case is a sound)
	//It has an attack coefficient, a hold val (in samples)
	//and a release coefficient. Finally, it has a trigger input.
	//If you stick a 1 in the trigger input, it retriggers the envelope
	sampleOut=envelope.ar(sound1.play(1.), 0.1, 0.9999, 1, sampleTrigger); //
		
	output[0]=sampleOut;//left channel
	output[1]=sampleOut;//right channel
	
	sampleTrigger = 0;//set trigger to 0 at the end of each sample to guarantee retriggering.

}