// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "BabyElephantWalk60_mp3.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

class PitchedAudioStream : public AudioStream {
public:
    PitchedAudioStream(AudioStream &out) : AudioStream(), _out(out), _rate(1.25) {
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
            newData[index++] = interpolated;
            /*
            printf("sample[%d]: %d\t\t\tinterpolation = {%d, %d, %d, %d}\t\t\tremainder = %f === %d\n",
                   wholeNumber,
                   samples[wholeNumber],
                   interpolationData[0],
                   interpolationData[1],
                   interpolationData[2],
                   interpolationData[3],
                   remainder,
                   interpolated);
            */
            int lastWholeNumber = wholeNumber;
            wholeNumber = floor(position);
            remainder = position - wholeNumber;
            position += _rate;

            if (wholeNumber - lastWholeNumber > 0) {
                interpolationData[0] = dataAsInt[lastWholeNumber];

                if (lastWholeNumber + 1 < numSamples)
                    interpolationData[1] = dataAsInt[lastWholeNumber + 1];
                else
                    interpolationData[1] = 0;

                if (lastWholeNumber + 2 < numSamples)
                    interpolationData[2] = dataAsInt[lastWholeNumber + 2];
                else
                    interpolationData[2] = 0;

                if (lastWholeNumber + 3 < numSamples)
                    interpolationData[3] = dataAsInt[lastWholeNumber + 3];
                else
                    interpolationData[3] = 0;
            }
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
    int16_t interpolationData[4] = {0,0,0,0};

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

