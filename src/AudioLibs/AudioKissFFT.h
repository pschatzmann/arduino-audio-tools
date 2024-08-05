#pragma once

#include "kiss_fix.h" 
#include "AudioFFT.h"

/** 
 * @defgroup fft-kiss KISS
 * @ingroup fft
 * @brief FFT using KISS 
**/

namespace audio_tools {

/**
 * @brief Driver for RealFFT
 * @ingroup fft-kiss
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FFTDriverKissFFT : public FFTDriver {
    public:
        bool begin(int len) override {
            this->len = len;
            if (p_fft_object==nullptr) p_fft_object = cpp_kiss_fft_alloc(len,false,nullptr,nullptr);
            if (p_data==nullptr) p_data = new kiss_fft_cpx[len];
            assert(p_fft_object!=nullptr);
            assert(p_data!=nullptr);
            return p_fft_object!=nullptr && p_data!=nullptr;
        }   

        void end() override {
            if (p_fft_object!=nullptr) kiss_fft_free(p_fft_object);
            if (p_fft_object_inv!=nullptr) kiss_fft_free(p_fft_object_inv);

            if (p_data!=nullptr) delete[] p_data;
            p_fft_object = nullptr;
            p_data = nullptr;
        }
        void setValue(int idx, float value) override {
            p_data[idx].r  = value; 
        }

        void fft() override {
           cpp_kiss_fft (p_fft_object, p_data, p_data);    
        };

        void rfft() override {
           if(p_fft_object_inv==nullptr) {
            p_fft_object_inv = cpp_kiss_fft_alloc(len,true,nullptr,nullptr);
           }
           cpp_kiss_fft (p_fft_object_inv, p_data, p_data);    
        };

        float magnitude(int idx) override { 
            return sqrt(magnitudeFast(idx));
        }

        /// magnitude w/o sqrt
        float magnitudeFast(int idx) override { 
            return (p_data[idx].r * p_data[idx].r + p_data[idx].i * p_data[idx].i);
        }

        bool isValid() override{ return p_fft_object!=nullptr; }

        bool isReverseFFT() override {return true;}

        float getValue(int idx) override { return p_data[idx].r };

        bool setBin(int pos, float real, float img) override {
            if (pos>=len) return false;
            p_data[pos].r = real;
            p_data[pos].i = img;
            return true;
        }
        bool getBin(int pos, FFTBin &bin) override { 
            if (pos>=len) return false;
            bin.real = p_data[idx].r;
            bin.img = p_data[idx].i;
            return true;
        }

        kiss_fft_cfg p_fft_object=nullptr;
        kiss_fft_cfg p_fft_object_inv=nullptr;
        kiss_fft_cpx *p_data = nullptr; // real
        int len = 0;

};
/**
 * @brief AudioFFT using FFTReal. The only specific functionality is the access to the dataArray
 * @ingroup fft-kiss
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioKissFFT : public AudioFFTBase {
    public:
        AudioKissFFT():AudioFFTBase(new FFTDriverKissFFT()) {}

        /// Provides the complex array returned by the FFT  
        kiss_fft_cpx *dataArray() {
            return driverEx()->p_data;
        }

        FFTDriverKissFFT* driverEx() {
            return (FFTDriverKissFFT*)driver();
        }
};


}