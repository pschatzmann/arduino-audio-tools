#include "AudioTools.h"
#include "AudioTools/AudioLibs/MaximilianDSP.h"

// Define Arduino output
I2SStream out;
Maximilian maximilian(out);

//This shows how to use maximilian to do basic amplitude modulation. Amplitude modulation is when you multiply waves together. In maximilian you just use the * inbetween the two waveforms.

maxiOsc mySine,myOtherSine;//Two oscillators. They can be called anything. They can be any of the available waveforms. These ones will be sinewaves

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
    
    // This form of amplitude modulation is straightforward multiplication of two waveforms.
    // Notice that the maths is different to when you add waves.
    // The waves aren't 'beating'. Instead, the amplitude of one is modulating the amplitude of the other
    // Remember that the sine wave has positive and negative sections as it oscillates.
    // When you multiply something by -1, its phase is inverted but it retains its amplitude.
    // So you hear 2 waves per second, not 1, even though the frequency is 1.
    output[0]=mySine.sinewave(440)*myOtherSine.sinewave(1);
    output[1]=output[0];
}

// Arduino loop
void loop() {
    maximilian.copy();
}