// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3MAD.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "BabyElephantWalk60_mp3.h"

using namespace audio_tools;  

MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
PortAudioStream portaudio_stream;   // Output of sound on desktop 
EncodedAudioStream dec(&portaudio_stream, new MP3DecoderMAD()); // MP3 data source
StreamCopy copier(dec, mp3); // copy in to out

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  dec.addNotifyAudioChange(portaudio_stream);
  dec.begin();

  portaudio_stream.begin();
}

void loop(){
  if (mp3) {
    copier.copy();
  } else {
    auto info = dec.decoder().audioInfo();
    LOGI("The audio rate from the mp3 file is %d", info.sample_rate);
    LOGI("The channels from the mp3 file is %d", info.channels);
    exit(0);
  }
}

