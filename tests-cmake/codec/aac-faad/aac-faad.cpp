#include "Arduino.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecAACFAAD.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "audio.h"

using namespace audio_tools;  

MemoryStream aac(gs_16b_2c_44100hz_aac, gs_16b_2c_44100hz_aac_len);
PortAudioStream out;   // Output of sound on desktop 
//CsvOutput<int16_t> out(Serial, 2);
EncodedAudioStream dec(&out, new AACDecoderFAAD()); // aac data source
StreamCopy copier(dec, aac); // copy in to out

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  dec.begin();

  out.begin();
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

