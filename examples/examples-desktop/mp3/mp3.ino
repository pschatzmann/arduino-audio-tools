// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "BabyElephantWalk60_mp3.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

class PitchedAudioStream : public AudioStream {
public:
    PitchedAudioStream(AudioStream &out) : AudioStream(), _out(out), _rate(0.75) {
        //setAudioInfo(AudioInfo(44100, 2, 16));
    }
    virtual ~PitchedAudioStream() = default;
    PitchedAudioStream(PitchedAudioStream &) = default;
    PitchedAudioStream &operator=(PitchedAudioStream &) = default;

    size_t write(const uint8_t *data, size_t len) override{
        size_t newSize = ceil(len / _rate);


        int16_t* dataAsInt = (int16_t*)data;
        for (int i=0; i<newSize/2; i++) {
            int index = round( i * _rate);
            newData[i] = dataAsInt[index];
        }

         int result = writeData(&_out, (uint8_t*)newData, newSize);
         return len;
    }

    bool begin() override {
        //setAudioInfo(AudioInfo(22050, 1, 16));
        _out.setAudioInfo(AudioInfo(22050, 1, 16));
        return true;
    }

private:
    AudioStream &_out;
    double _rate;
    int16_t newData[20000];
};

MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
PortAudioStream out;   // Output of sound on desktop
//ResampleStreamT<BSplineInterpolator> resample(out);
PitchedAudioStream pitchedAudioStream(out);
EncodedAudioStream dec(&pitchedAudioStream, new MP3DecoderHelix()); // MP3 data source
//ResampleStream resample(dec);
StreamCopy copier(dec, mp3); // copy in to out

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  
  out.begin();
  mp3.begin();
  dec.begin();
  pitchedAudioStream.begin();
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

