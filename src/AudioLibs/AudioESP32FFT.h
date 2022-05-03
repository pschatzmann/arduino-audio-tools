#pragma once

#include "AudioFFT.h"
#include "fft.h"

namespace audio_tools {

/**
 * @brief Driver for ESP32-FFT https://github.com/pschatzmann/esp32-fft  
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FFTDriverESP32FFT : public FFTDriver {
    public:
        void begin(int len) override {
            this->len = len;
            if (p_fft_object==nullptr) p_fft_object = fft_init(len, FFT_REAL, FFT_FORWARD, NULL, NULL);
        }
        void end()override{
            if (p_fft_object!=nullptr) fft_destroy(p_fft_object);
        }
        void setValue(int idx, int value) override{
            p_fft_object->input[idx]  = value; 
        }

        void fft() override{
            fft_execute(p_fft_object);
        };

        float magnitude(int idx) override {
            return sqrt(pow(p_fft_object->output[2*idx],2) + pow(p_fft_object->output[2*idx+1],2))/1;;
        }

        virtual bool isValid() override{ return p_fft_object!=nullptr; }

        fft_config_t *p_fft_object=nullptr;
        int len;

};

/**
 * @brief AudioFFT using RealFFT
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
