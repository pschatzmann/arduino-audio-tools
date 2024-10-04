#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioSourceSTD.h"
#include "AudioTools/AudioLibs/StdioStream.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"

//StdioStream out; // Output to Desktop
PortAudioStream out; // Output to Desktop
const char *startFilePath="/home/pschatzmann/Downloads";
const char* ext="wav";
AudioSourceSTD source(startFilePath, ext);
WAVDecoder decoder;
AudioPlayer player(source, out, decoder);


void setup() {
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // setup output
  auto cfg = out.defaultConfig();
  out.begin(cfg);

  // setup player
  //source.setFileFilter("*Bob Dylan*");
  player.begin();
}

void loop() {
  player.copy();
}