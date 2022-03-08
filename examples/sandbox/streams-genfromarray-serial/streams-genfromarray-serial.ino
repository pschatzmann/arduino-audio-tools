#include "AudioTools.h"

int channels = 2;
int sample_rate = 5000;
int16_t array[] = {-16000, 0, 16000, 0 };
int repeat = 10; // 0= endless
GeneratorFromArray<int16_t> generator(array,  repeat, false);      // generator from table
GeneratedSoundStream<int16_t> sound(generator);             // Stream
CsvStream<int16_t> out(Serial);                             // Output
StreamCopy copier(out, sound);                              // copies sound to out
unsigned long timeout=0;

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // Define CSV Output
  auto config = out.defaultConfig();
  config.sample_rate = sample_rate; 
  config.channels = channels;
  out.begin(config);

  auto cfgGen = sound.defaultConfig();
  cfgGen.channels = channels;
  sound.begin(cfgGen); // setup channels
  //timeout = millis() + 1000;
}

void loop() {
  copier.copy();

  // trigger a singal every 5 seconds
  if (millis()>timeout && !generator.isRunning()){
    // reset generator
     generator.begin();
     timeout = millis() + 1000;   

  }
}