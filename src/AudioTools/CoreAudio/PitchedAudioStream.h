#pragma once

#include <AudioTools/Disk/AudioSourceSTD.h>
#include "Arduino.h"
#include "AudioTools.h"
#include <queue>

template<typename T>
void pop_front(std::vector<T>& vec)
{
    assert(!vec.empty());
    vec.erase(vec.begin());
}


class PitchedAudioStream : public AudioStream {
public:
    PitchedAudioStream(AudioStream &out) : AudioStream(), _out(out), _rate(1.0) {
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
                if (buffer.size() > 4)
                    pop_front(buffer);
                interpolationData[0] = buffer[3];
                interpolationData[1] = buffer[2];
                interpolationData[2] = buffer[1];
                interpolationData[3] = buffer[0];
            }
        }

        int result = writeData(&_out, (uint8_t*)newData, newSize);
        return len;
    }

    bool begin() override {
        buffer.clear();
        for (int i=0; i<4; i++)
            buffer.push_back(0);
        return true;
    }

    void end() override {
        buffer.clear();
    }
    void setRate(double rate) {
        _rate = rate;
    }
    double getRate() {
        return _rate;
    }
private:
    AudioStream &_out;
    double _rate;
    int16_t newData[20000];
    int16_t interpolationData[4] = {0,0,0,0};
    std::queue<int16_t> outgoing;
    std::vector<int16_t> buffer;

    static int16_t fastinterpolate(int16_t d1, int16_t d2, int16_t d3, int16_t d4, double x) {
        double x_1 = x * 1000.0;
        double x_2 = x_1 * x_1;
        double x_3 = x_2 * x_1;

        return d1 * (x_3  - 6000.0 * x_2   + 11000000.0  * x_1  - 6000000000.0 ) / - 6000000000.0
               + d2 * (x_3  - 5000.0 * x_2   +  6000000.0  * x_1        )     /   2000000000.0
               + d3 * (x_3  - 4000.0 * x_2   +  3000000.0  * x_1        )     / - 2000000000.0
               + d4 * (x_3  - 3000.0 * x_2   +  2000000.0  * x_1        )     /   6000000000.0;
    }
};
