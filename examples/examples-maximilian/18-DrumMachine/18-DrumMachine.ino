#include "AudioTools.h"
#include "AudioLibs/MaximilianDSP.h"
#include "audio/kick.h"
#include "audio/snare.h"

// Arduino output
I2SStream out;
Maximilian maximilian(out);

// Maximilian def
maxiSample kick,snare; //we've got two sampleplayers
maxiOsc timer; //and a timer

int currentCount,lastCount,playHead,hit[16]={1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1}; //This is the sequence for the kick
int snarehit[16]={0,0,0,0,1,0,0,0,0,0,0,0,1,1,0,0};//This is the sequence for the snare

int kicktrigger,snaretrigger;

double sampleOut;

void setup() {//some inits
    // setup logging
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    // setup audio output
    auto cfg = out.defaultConfig(TX_MODE);
    out.begin(cfg);
    maximilian.begin(cfg);
	
    //YOU HAVE TO PROVIDE THE SAMPLES: we assign data in progmem    
	//kick.load("/sdcard/Maximilian/kick.wav");//load in your samples. Provide the full path to a wav file.
    //snare.load("/sdcard/Maximilian/snare.wav");
	kick.setSample(kick_vector);
	snare.setSample(snare_vector);
	
	
	printf("Summary:\n%s", kick.getSummary());//get info on samples if you like.
	//beats.getLength();
}

void play(float *output) {//this is where the magic happens. Very slow magic.
	
	currentCount=(int)timer.phasor(8);//this sets up a metronome that ticks 8 times a second
	
	if (lastCount!=currentCount) {//if we have a new timer int this sample, play the sound
		
		kicktrigger=hit[playHead%16];//get the value out of the array for the kick
		snaretrigger=snarehit[playHead%16];//same for the snare
		playHead++;//iterate the playhead
		lastCount=0;//reset the metrotest
	}
	
	if (kicktrigger==1) {//if the sequence has a 1 in it
		kick.trigger();//reset the playback position of the sample to 0 (the beginning)
	}
	
	if (snaretrigger==1) {
		snare.trigger();//likewise for the snare
	}
	
	sampleOut=kick.playOnce()+snare.playOnce();//just play the file. No looping.
	
	output[0]=sampleOut;//left channel
	output[1]=sampleOut;//right channel
	
	kicktrigger = 0;//set trigger to 0 at the end of each sample to guarantee retriggering.
	snaretrigger = 0;
}

// Arduino loop
void loop() {
    maximilian.copy();
}

