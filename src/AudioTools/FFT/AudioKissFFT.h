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
            k_data.resize(len);
            if (p_fft_object==nullptr) p_fft_object = cpp_kiss_fft_alloc(len,false,nullptr,nullptr);
            assert(p_fft_object!=nullptr);
            return p_fft_object!=nullptr;
        }   

        void end() override {
            if (p_fft_object!=nullptr) kiss_fft_free(p_fft_object);
            if (p_fft_object_inv!=nullptr) kiss_fft_free(p_fft_object_inv);

            p_fft_object = nullptr;
            k_data.resize(0);
        }
        void setValue(int idx, float value) override {
            k_data[idx].r  = value; 
        }

        void fft() override {
           cpp_kiss_fft (p_fft_object, k_data.data(), k_data.data());    
        };

        void rfft() override {
           if(p_fft_object_inv==nullptr) {
            p_fft_object_inv = cpp_kiss_fft_alloc(len,true,nullptr,nullptr);
           }
           cpp_kiss_fft (p_fft_object_inv, k_data.data(), k_data.data());    
        };

        float magnitude(int idx) override { 
            return sqrt(magnitudeFast(idx));
        }

        /// magnitude w/o sqrt
        float magnitudeFast(int idx) override { 
            return (k_data[idx].r * k_data[idx].r + k_data[idx].i * k_data[idx].i);
        }

        bool isValid() override{ return p_fft_object!=nullptr; }

        bool isReverseFFT() override {return true;}

        float getValue(int idx) override { return k_data[idx].r; }

        bool setBin(int pos, FFTBin &bin)  { return FFTDriver::setBin(pos, bin);}

        bool setBin(int pos, float real, float img) override {
            if (pos>=len) return false;
            k_data[pos].r = real;
            k_data[pos].i = img;
            return true;
        }
        bool getBin(int pos, FFTBin &bin) override { 
            if (pos>=len) return false;
            bin.real = k_data[pos].r;
            bin.img = k_data[pos].i;
            return true;
        }

        kiss_fft_cfg p_fft_object=nullptr;
        kiss_fft_cfg p_fft_object_inv=nullptr;
        Vector<kiss_fft_cpx> k_data{0}; // real
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
            return driverEx()->k_data.data();
        }

        FFTDriverKissFFT* driverEx() {
            return (FFTDriverKissFFT*)driver();
        }
};


}