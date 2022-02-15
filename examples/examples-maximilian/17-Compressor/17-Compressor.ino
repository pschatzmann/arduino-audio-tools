
#include "AudioTools.h"
#include "AudioLibs/MaximilianDSP.h"
#include <FS.h>
#include <SD_MMC.h>


// Arduino output
I2SStream out;
Maximilian maximilian(out);

// Maximilian
maxiSample beats; //We give our sample a name. It's called beats this time. We could have loads of them, but they have to have different names.
maxiDyn compressor; //this is a compressor
float fout;

void setup() {//some inits
    // setup logging
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);

    // setup audio output
    auto cfg = out.defaultConfig(TX_MODE);
    out.begin(cfg);
    maximilian.begin(cfg);

    // setup SD to allow file operations
    if(!SD_MMC.begin()){
        Serial.println("Card Mount Failed");
        return;
    }

    //load in your samples. Provide the full path to a wav file: beat2 is too big
    //beats.load("/sdcard/Maximilian/beat2.wav");
    beats.load("/sdcard/Maximilian/snare.wav");
    Serial.println(beats.getSummary().c_str());//get info on samples if you like.
    
    compressor.setAttack(100);
    compressor.setRelease(300);
    compressor.setThreshold(0.25);
    compressor.setRatio(5);
    
    //you can set these any time you like.
    
}

void play(float *output) {//this is where the magic happens. Very slow magic.
    //here, we're just compressing the file in real-time
    //arguments are input,ratio,threshold,attack,release
    fout=compressor.compress(beats.play());
    
    output[0]=fout;
    output[1]=fout;    
}

// Arduino loop
void loop() {
    maximilian.copy();
}
