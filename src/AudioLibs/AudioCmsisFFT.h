#pragma once
#include "AudioFFT.h"
#ifdef STM32
#  include "CMSIS_DSP.h"
#endif
#if defined(ARDUINO_ARCH_RENESAS) || defined(ARDUINO_ARCH_RP2040)
#  include "arm_vec_fft.h"
#endif
/** 
 * @defgroup fft-cmsis CMSIS
 * @ingroup fft
 * @brief FFT using CMSIS 
**/

namespace audio_tools {

/**
 * @brief Driver for Cmsis-FFT see https://arm-software.github.io/CMSIS_5/DSP
 * @ingroup fft-cmsis
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FFTDriverCmsisFFT : public FFTDriver {
    public:
        bool begin(int len) override {
            TRACEI();
            this->len = len;
            input = new float[len];
            output = new float[len*2];
            output_magn = new float[len];
	        status = arm_rfft_fast_init_f32(&fft_instance, len);
            if (status!=ARM_MATH_SUCCESS){
                LOGE("arm_rfft_fast_init_f32: %d", status);
            }
            assert(input!=nullptr);
            assert(output!=nullptr);
            assert(output_magn != nullptr);
            return input!=nullptr && output != nullptr && output_magn != nullptr;
        }
        void end()override{
            TRACEI();
            if (input!=nullptr) delete input;
            if (output!=nullptr) delete output;
            if (output_magn!=nullptr) delete output_magn;
            input = nullptr;
            output = nullptr;
            output_magn = nullptr;
        }

        void setValue(int idx, int value) override{
            input[idx]  = value; 
        }

        void fft() override {
            TRACED();
		    arm_rfft_fast_f32(&fft_instance, input, output, ifft);
		    arm_cmplx_mag_f32(output, output_magn, len / 2);
            /* Calculates maxValue and returns corresponding BIN value */
            arm_max_f32(output_magn, len / 2, &result_max_value, &result_index);
            TRACED();
        };

        float magnitude(int idx) override {
            return output_magn[idx];
        }

        /// same as magnitude
        float magnitudeFast(int idx) override {
            return output_magn[idx];
        }

        virtual bool isValid() override{ return status==ARM_MATH_SUCCESS; }

	    arm_rfft_fast_instance_f32 fft_instance;
    	arm_status status;
        int len;
        bool ifft = false;
        float *input=nullptr;
        float *output_magn=nullptr;
        float *output=nullptr;
        float result_max_value;
        uint32_t result_index = 0;

};

/**
 * @brief AudioFFT for ARM processors that provided Cmsis  DSP
 * @ingroup fft-cmsis
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioCmsisFFT : public AudioFFTBase {
    public:
        AudioCmsisFFT():AudioFFTBase(new FFTDriverCmsisFFT()) {}

        /// Provides the result array returned by CMSIS FFT
        float* array() {
            return driverEx()->output;
        }

        float* magnitudes() {
            return driverEx()->output_magn;
        }

        AudioFFTResult result() {
            AudioFFTResult ret_value;
            ret_value.magnitude = driverEx()->result_max_value;
            ret_value.bin = driverEx()->result_index;
            return ret_value;
        }

        FFTDriverCmsisFFT* driverEx() {
            return (FFTDriverCmsisFFT*)driver();
        }
};


}