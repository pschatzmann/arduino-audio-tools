#include "AudioTools.h"
#include "AudioLibs/MaximilianDSP.h"
#include "AudioLibs/AudioBoardStream.h"
#include "maximilian.h"
#include <FS.h>
#include <SD_MMC.h>


// Define Arduino output
AudioBoardStream out(AudioKitEs8388V1);
Maximilian maximilian(out);

// MAXIMILIAN
maxiSample beats; //We give our sample a name. It's called beats this time. We could have loads of them, but they have to have different names.

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

    //load in your samples: beat2.wav is too big - but snare.wav will work
    //beats.load("/sdcard/Maximilian/beat2.wav");
    beats.load("/sdcard/Maximilian/snare.wav");
    //get info on samples if you like.
    Serial.println(beats.getSummary().c_str());

}

//this is where the magic happens. Very slow magic.
void play(float *output) {
    //output[0]=beats.play();//just play the file. Looping is default for all play functions.
    output[0]=beats.playAtSpeed(0.68);//play the file with a speed setting. 1. is normal speed.
    //output[0]=beats.playAtSpeedBetweenPoints(0.5,0,13300);//linear interpolationplay with a frequency input, start point and end point. Useful for syncing.
    //output[0]=beats.play4(0.5,0,13300);//cubic interpolation play with a frequency input, start point and end point. Useful for syncing.
    
    output[1]=output[0];
}

// Arduino loop
void loop() {
    maximilian.copy();
}
