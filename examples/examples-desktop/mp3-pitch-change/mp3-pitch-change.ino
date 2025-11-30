#include "Arduino.h"
#include "AudioTools.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

PortAudioStream out;   // Output of sound on desktop
PitchedAudioStream pitchedAudioStream(out);
AudioSourceSTD source;
MP3DecoderHelix dec;
EncodedAudioStream encodedAudioStream(&pitchedAudioStream, &dec); // MP3 data source
AudioPlayer player(source, pitchedAudioStream, dec);
std::string currentPath;

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  source.setTimeoutAutoNext(1000);
  currentPath = std::string( std::filesystem::current_path() ) + "/";
  source.setPath(currentPath.c_str());
  source.setFileFilter("*.mp3");
  source.begin();
  out.begin();
  dec.begin();
  player.begin();
  encodedAudioStream.begin();
  player.play();
  pitchedAudioStream.begin();
  pitchedAudioStream.setRate(0.5);
}

void loop(){
  if (out) {
      player.copy();
  } else {
    auto info = dec.audioInfo();
    LOGI("The audio rate from the mp3 file is %d", info.sample_rate);
    LOGI("The channels from the mp3 file is %d", info.channels);
    exit(0);
  }
}

