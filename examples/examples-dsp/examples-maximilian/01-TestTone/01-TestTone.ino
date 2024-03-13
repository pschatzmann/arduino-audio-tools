#include "AudioTools.h"
#include "AudioLibs/MaximilianDSP.h"

// Define output
I2SStream out;
Maximilian maximilian(out);

//This shows how the fundamental building block of digital audio - the sine wave.
maxiOsc mySine;//One oscillator - can be called anything. Can be any of the available waveforms.

void setup() {//some inits
  // setup logging
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);
  // setup Aduio output
  auto cfg = out.defaultConfig(TX_MODE);
  out.begin(cfg);
  maximilian.begin(cfg);
}

void play(float *output) {
    output[0]=mySine.sinewave(440);
    output[1]=output[0];
}

// Arduino loop
void loop() {
    maximilian.copy();
}