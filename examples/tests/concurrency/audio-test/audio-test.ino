
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Concurrency/RTOS.h"

AudioInfo info(44100, 2, 16);
// source and sink
SineWaveGenerator<int16_t> sineWave(32000);               
GeneratedSoundStream<int16_t> sound(sineWave); 
AudioBoardStream out(AudioKitEs8388V1);
// queue
// SynchronizedNBuffer buffer(1024, 10);
BufferRTOS<uint8_t> buffer(1024 * 10);
QueueStream<uint8_t> queue(buffer);
// copy
StreamCopy copierSource(queue, sound);                            
StreamCopy copierSink(out, queue);                            
// tasks
Task writeTask("write", 3000, 10, 0);
Task readTask("read", 3000, 10, 1);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);
  // start Queue
  queue.begin();

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info); 
  out.begin(config);

  // Setup sine wave
  sineWave.begin(info, N_B4);
  Serial.println("started...");

  writeTask.begin([]() {
    copierSource.copy();
  });

  readTask.begin([]() {
    copierSink.copy();
  });

  Serial.println("started...");

}

void loop() { delay(1000); }