// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include <AudioTools/Disk/AudioSourceSTD.h>
#include "Arduino.h"
#include "AudioTools.h"
#include "BabyElephantWalk60_mp3.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include <queue>

template<typename T>
void pop_front(std::vector<T>& vec)
{
    assert(!vec.empty());
    vec.erase(vec.begin());
}

class PitchedAudioStream : public AudioStream {
public:
    PitchedAudioStream(AudioStream &out) : AudioStream(), _out(out), _rate(1.50) {
        //setAudioInfo(AudioInfo(44100, 2, 16));
    }
    virtual ~PitchedAudioStream() = default;
    PitchedAudioStream(PitchedAudioStream &) = default;
    PitchedAudioStream &operator=(PitchedAudioStream &) = default;

    size_t write(const uint8_t *data, size_t len) override{
        size_t newSize = ceil(len / _rate);

        double position = 0;
        int wholeNumber = 0;
        double remainder = 0;
        uint32_t numSamples = len / 2;
        int index = 0;
        auto* dataAsInt = (int16_t*)data;
        while (wholeNumber < numSamples) {

            int interpolated = fastinterpolate(
                    interpolationData[0],
                    interpolationData[1],
                    interpolationData[2],
                    interpolationData[3],
                    1.0f + remainder);
            newData[index++] = (int16_t)interpolated;

            int lastWholeNumber = wholeNumber;
            wholeNumber = floor(position);
            remainder = position - wholeNumber;
            position += _rate;
            if (lastWholeNumber != wholeNumber) {
                buffer.push_back(dataAsInt[lastWholeNumber]);
                if (buffer.size() > 32)
                    pop_front(buffer);
                size_t bufferSize = buffer.size();
                interpolationData[0] = buffer[bufferSize - 1];
                interpolationData[1] = buffer[bufferSize - 2];
                interpolationData[2] = buffer[bufferSize - 3];
                interpolationData[3] = buffer[bufferSize - 4];
            }
        }

         int result = writeData(&_out, (uint8_t*)newData, newSize);
         return len;
    }

    bool begin() override {
        //setAudioInfo(AudioInfo(22050, 1, 16));
        //_out.setAudioInfo(AudioInfo(22050, 1, 16));
        for (int i=0; i<8; i++)
            buffer.push_back(0);
        return true;
    }

private:
    AudioStream &_out;
    double _rate;
    int16_t newData[20000];
    int16_t interpolationData[4] = {0,0,0,0};
    std::queue<int16_t> outgoing;
    std::vector<int16_t> buffer;

    int16_t fastinterpolate(int16_t d1, int16_t d2, int16_t d3, int16_t d4, double x) {
        float x_1 = x * 1000.0;
        float x_2 = x_1 * x_1;
        float x_3 = x_2 * x_1;

        return d1 * (x_3  - 6000 * x_2   + 11000000  * x_1  - 6000000000 ) / - 6000000000
               + d2 * (x_3  - 5000 * x_2   +  6000000  * x_1        )     /   2000000000
               + d3 * (x_3  - 4000 * x_2   +  3000000  * x_1        )     / - 2000000000
               + d4 * (x_3  - 3000 * x_2   +  2000000  * x_1        )     /   6000000000;
    }
};

//MemoryStream mp3(BabyElephantWalk60_mp3, BabyElephantWalk60_mp3_len);
PortAudioStream out;   // Output of sound on desktop
PitchedAudioStream pitchedAudioStream(out);
AudioSourceSTD source("/Users/nicholasnewdigate/Development/github/newdigate/arduino-audio-tools/examples/examples-desktop/mp3/cmake-build-debug/",".mp3");
//CopyDecoder dec; // no decoding, just copy
MP3DecoderHelix dec;
EncodedAudioStream encodedAudioStream(&pitchedAudioStream, &dec); // MP3 data source


AudioPlayer player(source, pitchedAudioStream, dec);

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  source.setTimeoutAutoNext(1000);
  source.begin();
  out.begin();
  dec.begin();
  player.begin();
  encodedAudioStream.begin();
  player.play();
  //pitchedAudioStream.begin();
}

void loop(){
  if (out) {
      //encodedAudioStream.
      player.copy();
  } else {
    auto info = dec.audioInfo();
    LOGI("The audio rate from the mp3 file is %d", info.sample_rate);
    LOGI("The channels from the mp3 file is %d", info.channels);
    exit(0);
  }
}

