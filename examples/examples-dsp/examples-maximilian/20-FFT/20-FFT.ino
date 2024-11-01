
// WARNNG: This sketch is too big to fit on an ESP32 w/o PSRAM !


#include "AudioTools.h"
#include "AudioTools/AudioLibs/MaximilianDSP.h"
#include "libs/maxim.h"

// Define Arduino output
I2SStream out;
Maximilian maximilian(out);

// Maximilian
maxiOsc mySine, myPhasor; // This is the oscillator we will use to generate the test tone
maxiFFT myFFT;

void setup() {
    // setup logging
    Serial.begin(115200);
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

    // setup audio output
    auto cfg = out.defaultConfig(TX_MODE);
    out.begin(cfg);
    maximilian.begin(cfg);

    // setup maximilian       
    myFFT.setup(512, 512, 256);
    
}

void play(float *output) {    
    float myOut=mySine.sinewave(myPhasor.phasorBetween(0.2,100,5000));
    //output[0] is the left output. output[1] is the right output
    
    if (myFFT.process(myOut)) {
        //if you want you can mess with FFT frame values in here
        
    }
    
    output[0]=myOut;//simple as that!
    output[1]=output[0];
}


// Arduino loop
void loop() {
    maximilian.copy();
}
