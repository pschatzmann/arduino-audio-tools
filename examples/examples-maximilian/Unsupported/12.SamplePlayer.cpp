#include "maximilian.h"

maxiSample beats; //We give our sample a name. It's called beats this time. We could have loads of them, but they have to have different names.

void setup() {//some inits
    
    beats.load("/Users/michaelgrierson/Documents/workspace/Maximilian/beat2.wav");//load in your samples. Provide the full path to a wav file.
    printf("Summary:\n%s", beats.getSummary());//get info on samples if you like.
    
}

void play(double *output) {//this is where the magic happens. Very slow magic.
    
    //output[0]=beats.play();//just play the file. Looping is default for all play functions.
    output[0]=beats.play(0.68);//play the file with a speed setting. 1. is normal speed.
    //output[0]=beats.play(0.5,0,44100);//linear interpolationplay with a frequency input, start point and end point. Useful for syncing.
    //output[0]=beats.play4(0.5,0,44100);//cubic interpolation play with a frequency input, start point and end point. Useful for syncing.
    
    output[1]=output[0];
}

