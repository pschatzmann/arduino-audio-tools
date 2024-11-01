#include "AudioTools.h"
#include "AudioTools/AudioLibs/MaximilianDSP.h"

// Define Arduino output
I2SStream out;
Maximilian maximilian(out);

// Maximilian
maxiOsc myOsc,myAutoPanner;//
vector<float> myStereoOutput(2,0);
maxiMix myOutputs;//this is the stereo mixer channel.

void setup() {//some inits
    // setup logging
    Serial.begin(115200);
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

    // setup audio output
    auto cfg = out.defaultConfig(TX_MODE);
    out.begin(cfg);
    maximilian.begin(cfg);
}

void play(float *output) {
    myOutputs.stereo(myOsc.noise(),myStereoOutput,(myAutoPanner.sinewave(1)+1)/2);//Stereo, Quad or 8 Channel. Specify the input to be mixed, the output[numberofchannels], and the pan (0-1,equal power).
    output[0]=myStereoOutput[0];//When working with mixing, you need to specify the outputs explicitly
    output[1]=myStereoOutput[1];//
}

// Arduino loop
void loop() {
    maximilian.copy();
}
