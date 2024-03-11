#include "Arduino.h"
#include "AudioTools.h"
#include "AudioCodecs/CodecAACFDK.h"
#include "AudioLibs/PortAudioStream.h"
#include "audio.h"

using namespace audio_tools;  

MemoryStream aac(gs_16b_2c_44100hz_aac, gs_16b_2c_44100hz_aac_len);
PortAudioStream portaudio_stream;   // Output of sound on desktop 
EncodedAudioStream dec(&portaudio_stream, new AACDecoderFDK()); // aac data source
StreamCopy copier(dec, aac); // copy in to out

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  dec.addNotifyAudioChange(portaudio_stream);
  dec.begin();

  portaudio_stream.begin();
}

void loop(){
  if (aac) {
    copier.copy();
  } else {
    auto info = dec.decoder().audioInfo();
    LOGI("The audio rate from the aac file is %d", info.sample_rate);
    LOGI("The channels from the aac file is %d", info.channels);
    exit(0);
  }
}

