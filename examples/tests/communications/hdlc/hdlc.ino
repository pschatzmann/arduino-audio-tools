#include "AudioTools.h"
#include "Communication/HDLCStream.h"
// #include "AudioLibs/AudioBoardStream.h"

AudioInfo info(44000, 2, 16);
I2SStream out; // or AudioBoardStream
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
QueueStream queue(1024,2); // we simulate a connection
HDLCStream hdlc_in(queue);
HDLCStream hdlc_out(queue);

StreamCopy copier_send(hdlc_out, sineWave );  
StreamCopy copier_receive(out, hdlc_in );  

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial2, AudioLogger::Warning);

  sineWave.begin(info, N_B4);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  out.begin(config);

  Serial.println("started...");
}

void loop() {
  copier_send.copy();
  copier_receive.copy();
}