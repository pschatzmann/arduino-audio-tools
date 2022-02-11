#include "maximilian.h"

maxiSample beats; //We give our sample a name. It's called beats this time. We could have loads of them, but they have to have different names.
maxiDyn compressor; //this is a compressor
double out;

void setup() {//some inits
    
    beats.load("/Users/michaelgrierson/Documents/workspace/Maximilian/ofxMaxim/examples/OSX/ofMaximExample007OSX_Granular/bin/data/beat2.wav");//load in your samples. Provide the full path to a wav file.
    printf("Summary:\n%s", beats.getSummary());//get info on samples if you like.
    
    compressor.setAttack(100);
    compressor.setRelease(300);
    compressor.setThreshold(0.25);
    compressor.setRatio(5);
    
    //you can set these any time you like.
    
}

void play(double *output) {//this is where the magic happens. Very slow magic.
    
    
    //here, we're just compressing the file in real-time
    //arguments are input,ratio,threshold,attack,release
    out=compressor.compress(beats.play());
    
    output[0]=out;
    output[1]=out;
    
}

