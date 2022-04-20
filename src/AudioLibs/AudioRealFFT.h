#pragma once

#include "AudioFFT.h"
#include "FFT/FFTReal.h"

namespace audio_tools {

/**
 * @brief Driver for RealFFT
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FFTDriverRealFFT : public FFTDriver {
    public:
        void begin(int len) override {
            this->len = len;
            if (p_fft_object==nullptr) p_fft_object = new ffft::FFTReal<float>(len);
            if (p_x==nullptr) p_x = new float[len];
            if (p_f==nullptr) p_f = new float[len];
        }
        void end()override{
            if (p_fft_object!=nullptr) delete p_fft_object;
            if (p_x!=nullptr) delete[] p_x;
            if (p_f!=nullptr) delete[] p_f;
        }
        void setValue(int idx, int value) override{
            p_x[idx] = value; 
        }

        void fft() override{
            memset(p_f,0,len*sizeof(float));
            p_fft_object->do_fft(p_f, p_x);    
        };

        float magnitude(int idx) override {
            return sqrt(p_x[idx] * p_x[idx] + p_f[idx] * p_f[idx]);
        }

        virtual bool isValid() override{ return p_fft_object!=nullptr; }

        ffft::FFTReal <float> *p_fft_object=nullptr;
        float *p_x = nullptr; // real
        float *p_f = nullptr; // complex
        int len;

};

/**
 * @brief AudioFFT using RealFFT
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioRealFFT : public AudioFFTBase {
    public:
        AudioRealFFT():AudioFFTBase(new FFTDriverRealFFT()) {}

        /// Provides the real array returned by the FFT
        float* realArray() {
            return driverEx()->p_x;
        }

        /// Provides the complex array returned by the FFT  
        float *complexArray() {
            return driverEx()->p_f;
        }

        FFTDriverRealFFT* driverEx() {
            return (FFTDriverRealFFT*)driver();
        }
};

}
