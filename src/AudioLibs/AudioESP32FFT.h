#pragma once

#include "AudioFFT.h"
#include "fft.h"

/** 
 * @defgroup fft-esp32 esp32-fft
 * @ingroup fft
 * @brief FFT using esp32-fft 
**/


namespace audio_tools {

/**
 * @brief Driver for ESP32-FFT https://github.com/pschatzmann/esp32-fft  
 * @ingroup fft-esp32
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FFTDriverESP32FFT : public FFTDriver {
    public:
        bool begin(int len) override {
            this->len = len;
            if (p_fft_object==nullptr) p_fft_object = fft_init(len, FFT_REAL, FFT_FORWARD, NULL, NULL);
            assert(p_fft_object!=nullptr);
            return p_fft_object!=nullptr;
        }
        void end()override{
            if (p_fft_object!=nullptr) fft_destroy(p_fft_object);
        }
        void setValue(int idx, float value) override{
            p_fft_object->input[idx]  = value; 
        }

        void fft() override{
            fft_execute(p_fft_object);
        };

        void rfft() override {
            irfft(p_fft_object->input, p_fft_object->output, p_fft_object->twiddle_factors, p_fft_object->size);
        }

        float magnitude(int idx) override {
            return sqrt(magnitudeFast(idx));
        }

        /// magnitude w/o sqrt
        float magnitudeFast(int idx) override {
            return (pow(p_fft_object->output[2*idx],2) + pow(p_fft_object->output[2*idx+1],2));
        }

        float getValue(int idx) { return p_fft_object->input[idx];}

        bool setBin(int pos, float real, float img) override {
            if (pos>=len) return false;
            p_fft_object->output[2*pos] = real;
            p_fft_object->output[2*pos+1] = img;
            return true;
        }
        bool getBin(int pos, FFTBin &bin) override { 
            if (pos>=len) return false;
            bin.real = p_fft_object->output[2*pos];
            bin.img = p_fft_object->output[2*pos+1];
            return true;
        }

        bool isReverseFFT() override {return true;}

        bool isValid() override{ return p_fft_object!=nullptr; }

        fft_config_t *p_fft_object=nullptr;
        int len;

};

/**
 * @brief AudioFFT using RealFFT
 * @ingroup fft-esp32
 * @author Phil Schatzmann
 * Warning: This does not work as expected yet: I did not get the expected results...
 * @copyright GPLv3
 */
class AudioESP32FFT : public AudioFFTBase {
    public:
        AudioESP32FFT():AudioFFTBase(new FFTDriverESP32FFT()) {}

        /// Provides the result array returned by the FFT: The real part of a magnitude at a frequency is followed by the corresponding imaginary part in the output*/
        float* array() {
            return driverEx()->p_fft_object->output;
        }

        FFTDriverESP32FFT* driverEx() {
            return (FFTDriverESP32FFT*)driver();
        }
};

}
