
#include "AudioTools.h"
#include "AudioTools/Concurrency/RP2040.h"

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
BufferRP2040 buffer(1024, 10); 
QueueStream queue(buffer);
CsvOutput<int16_t> csv(Serial);
StreamCopy copierFill(queue, sound);                             // copies sound into i2s
StreamCopy copierConsume(csv, queue);                             // copies sound into i2s


void setup(){
  Serial.begin(115200);
  while(!Serial);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  queue.begin();
  sineWave.begin(info, N_B4);
  csv.begin(info);
}

void loop(){
    copierFill.copy();
}

void loop1(){
    copierConsume.copy();
}