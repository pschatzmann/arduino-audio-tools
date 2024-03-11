#pragma once

#include "kiss_fft.h"
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
            if (p_fft_object==nullptr) p_fft_object = kiss_fft_alloc(len,0,nullptr,nullptr);
            if (p_data==nullptr) p_data = new kiss_fft_cpx[len];
            assert(p_fft_object!=nullptr);
            assert(p_data!=nullptr);
            return p_fft_object!=nullptr && p_data!=nullptr;
        }   

        void end() override {
            if (p_fft_object!=nullptr) kiss_fft_free(p_fft_object);
            if (p_data!=nullptr) delete[] p_data;
            p_fft_object = nullptr;
            p_data = nullptr;
        }
        void setValue(int idx, int value) override {
            p_data[idx].r  = value; 
        }

        void fft() override {
            kiss_fft (p_fft_object, p_data, p_data);    
        };

        float magnitude(int idx) override { 
            return sqrt(p_data[idx].r * p_data[idx].r + p_data[idx].i * p_data[idx].i);
        }

        /// magnitude w/o sqrt
        float magnitudeFast(int idx) override { 
            return (p_data[idx].r * p_data[idx].r + p_data[idx].i * p_data[idx].i);
        }

        virtual bool isValid() override{ return p_fft_object!=nullptr; }

        kiss_fft_cfg p_fft_object=nullptr;
        kiss_fft_cpx *p_data = nullptr; // real

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