#pragma once

#include "AudioFFT.h"
#include "FFT/FFTReal.h"

/** 
 * @defgroup fft-real Real
 * @ingroup fft
 * @brief FFT using Real FFT 
**/

namespace audio_tools {

/**
 * @brief Driver for RealFFT
 * @ingroup fft-real
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FFTDriverRealFFT : public FFTDriver {
    public:
        bool begin(int len) override {
            this->len = len;
            if (p_fft_object==nullptr) p_fft_object = new ffft::FFTReal<float>(len);
            if (p_x==nullptr) p_x = new float[len];
            if (p_f==nullptr) p_f = new float[len];
            assert(p_fft_object!=nullptr);
            assert(p_x!=nullptr);
            assert(p_f!=nullptr);
            return p_fft_object!=nullptr && p_x!=nullptr && p_f!=nullptr;
        }
        void end()override{
            if (p_fft_object!=nullptr) delete p_fft_object;
            if (p_x!=nullptr) delete[] p_x;
            if (p_f!=nullptr) delete[] p_f;
            p_fft_object = nullptr;
            p_x = nullptr;
            p_f = nullptr;
        }
        void setValue(int idx, float value) override{
            p_x[idx] = value; 
        }

        void fft() override{
            memset(p_f,0,len*sizeof(float));
            p_fft_object->do_fft(p_f, p_x);    
        };

        /// Inverse fft - convert fft result back to time domain (samples)
        void rfft() override{
           // ifft
           p_fft_object->do_ifft(p_f, p_x);
        }

        bool isReverseFFT() override { return true;}

        float magnitude(int idx) override {
            return sqrt(magnitudeFast(idx));
        }

        /// magnitude w/o sqrt
        float magnitudeFast(int idx) override {
            return (p_x[idx] * p_x[idx] + p_f[idx] * p_f[idx]);
        }

        bool isValid() override{ return p_fft_object!=nullptr; }

        /// get Real value
        float getValue(int idx) override { return p_x[idx];}

        bool setBin(int pos, float real, float img) override {
            if (pos>=len) return false;
            p_x[pos] = real;
            p_f[pos] = img;
            return true;
        }
        bool getBin(int pos, FFTBin &bin) override { 
            if (pos>=len) return false;
            bin.real = p_x[pos];
            bin.img = p_f[pos];
            return true;
        }

        ffft::FFTReal <float> *p_fft_object=nullptr;
        float *p_x = nullptr; // real
        float *p_f = nullptr; // complex
        int len;

};

/**
 * @brief AudioFFT using RealFFT
 * @ingroup fft-real
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioRealFFT : public AudioFFTBase {
    public:
        AudioRealFFT():AudioFFTBase(new FFTDriverRealFFT()) {}

        AudioRealFFT(Print &out):AudioFFTBase(new FFTDriverRealFFT()) {
            setOutput(out);
        }

        /// Provides the real array returned by the FFT
        float* realArray() {
            return driverEx()->p_x;
        }

        /// Provides the complex array returned by the FFT  
        float *imgArray() {
            return driverEx()->p_f;
        }

        FFTDriverRealFFT* driverEx() {
            return (FFTDriverRealFFT*)driver();
        }
};

}
