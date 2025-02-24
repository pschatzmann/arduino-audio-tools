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
            v_x.resize(len);
            v_f.resize(len);
            if (p_fft_object==nullptr) p_fft_object = new ffft::FFTReal<float>(len);
            assert(p_fft_object!=nullptr);
            return p_fft_object!=nullptr;
        }
        void end()override{
            if (p_fft_object!=nullptr) {
                delete p_fft_object;
                p_fft_object = nullptr;
            } 
            v_x.resize(0);
            v_f.resize(0);
        }
        void setValue(int idx, float value) override{
            v_x[idx] = value; 
        }

        void fft() override{
            memset(v_f.data(),0,len*sizeof(float));
            p_fft_object->do_fft(v_f.data(), v_x.data());    
        };

        /// Inverse fft - convert fft result back to time domain (samples)
        void rfft() override{
           // ifft
           p_fft_object->do_ifft(v_f.data(), v_x.data());
        }

        bool isReverseFFT() override { return true;}

        float magnitude(int idx) override {
            return sqrt(magnitudeFast(idx));
        }

        /// magnitude w/o sqrt
        float magnitudeFast(int idx) override {
            return ((v_x[idx] * v_x[idx]) + (v_f[idx] * v_f[idx]));
        }

        bool isValid() override{ return p_fft_object!=nullptr; }

        /// get Real value
        float getValue(int idx) override { return v_x[idx];}

        bool setBin(int pos, float real, float img) override {
            if (pos < 0 || pos >= len) return false;
            v_x[pos] = real;
            v_f[pos] = img;
            return true;
        }
        bool getBin(int pos, FFTBin &bin) override { 
            if (pos>=len) return false;
            bin.real = v_x[pos];
            bin.img = v_f[pos];
            return true;
        }

        ffft::FFTReal <float> *p_fft_object=nullptr;
        Vector<float> v_x{0}; // real
        Vector<float> v_f{0}; // complex
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

        /// Provides the real array returned by the FFT
        float* realArray() {
            return driverEx()->v_x.data();
        }

        /// Provides the complex array returned by the FFT  
        float *imgArray() {
            return driverEx()->v_f.data();
        }

        FFTDriverRealFFT* driverEx() {
            return (FFTDriverRealFFT*)driver();
        }
};

}
