#include "AudioTools.h"
#include "AudioTools/AudioLibs/MiniAudioStream.h"
#include "AudioTools/AudioLibs/FFTEffects.h"

AudioInfo info(16000, 2, 16);
MiniAudioStream out; // final output of decoded stream
FFTPitchShift effect(out);
//FFTNop effect(out);
//FFTWhisper effect(out);
//FFTRobotize effect(out);
WAVDecoder wav;
EncodedAudioOutput decoder(&effect, &wav); // Decoding stream
StreamCopy copier; 
File audioFile;

void setup(){
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  out.begin(config);

  auto econfig = effect.defaultConfig();
  econfig.copyFrom(info);
  econfig.shift = -20;
  effect.begin(econfig);

  // setup file: must be in current directory
  audioFile.open("hal1600.wav");

  // setup I2S based on sampling rate provided by decoder
  decoder.begin();

  // begin copy
  copier.begin(decoder, audioFile);

}

void loop(){
  if (!copier.copy()) {
    delay(5000);
    exit(0);
  }
}