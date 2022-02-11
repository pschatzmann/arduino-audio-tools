#include "maximilian.h"

// Here we define a double floating value that will contain our
// frame of lovely maximilian generated audio
double out;

// Our oscillator to fill our frame up with my favourite wave
maxiOsc osc;

// Our ramp to modulate the oscillators
maxiOsc ramp;

// We declare our recorder object here, which will call it's
// default constructor. 
maxiRecorder recorder;

void setup() {

    // Call setup here, make sure you do this so the recorder
    // knows where to write the file. Currently the recorder
    // will write the wav file to the directory that this file
    // is in if you use linux but with mac and windows I 
    // strongly reccomend putting an absolute file path to the
    // directory you want to write to. Also, when in Windows,
    // remember to do double '\' characters because they
    // count as an escape which will nullify any path you write
    recorder.setup("lovesong.wav");

    // This must be called to start the asynchronous thread to
    // manage the recorder's internal memory
    recorder.startRecording();
}

void play(double *output) {
    
    // A pulse wave!!! Yay
    out = osc.pulse(90, ramp.phasor(.2));
    
    // Fill our output buffer
    output[0]=out;
    output[1]=out;

    // After we have filled our output array, send the array
    // and the size of the array (in this case the amount of
    // channels, but in ofx or juce you might need to do 
    // something like channels*bufferSize).
    recorder.passData(output, maxiSettings::channels);
}

// We don't need to worry about telling the recorder to stop;
// when the stack unwinds and the maximillian program stops,
// the recorder will have its destructor called and the wav
// will be written for you. If you would like to do something
// more dynamic, look at the class definition in maximilian.h -
// the api allows for stricter control of the object.


