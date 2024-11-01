// Nothing much to say about this other than I like it.

#include "AudioTools.h"
#include "AudioTools/AudioLibs/MaximilianDSP.h"

// Define Arduino output
I2SStream out;
Maximilian maximilian(out);
// Maximilian 
maxiOsc mySine,myOtherSine,myLastSine,myPhasor;//Three oscillators


void setup() {//some inits
  // setup logging
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup Aduio output
  auto cfg = out.defaultConfig(TX_MODE);
  out.begin(cfg);
  maximilian.begin(cfg);
}

void play(float *output) {
    output[0]=mySine.sinewave(myOtherSine.sinewave(myLastSine.sinewave(0.1)*30)*440);//awesome bassline
    output[1]=output[0];
}

// Arduino loop
void loop() {
    maximilian.copy();
}