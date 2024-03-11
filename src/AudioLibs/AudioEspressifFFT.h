#pragma once

#include "AudioFFT.h"
#include "esp_dsp.h"

/** 
 * @defgroup fft-dsp esp32-dsp
 * @ingroup fft
 * @brief FFT using esp32 esp-dsp library 
**/

namespace audio_tools {

/**
 * @brief fft Driver for espressif dsp library: https://espressif-docs.readthedocs-hosted.com/projects/esp-dsp/en/latest/esp-dsp-apis.html
 * @ingroup fft-dsp
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FFTDriverEspressifFFT : public FFTDriver {
    public:
        bool begin(int len) override {
            N = len;
            if (p_data==nullptr){
                p_data = new float[len*2];
                if (p_data==nullptr){
                    LOGE("not enough memory");
                }
            }
            assert(p_data!=nullptr);
            ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
            if (ret  != ESP_OK){
                LOGE("dsps_fft2r_init_fc32 %d", ret);
            }
            return p_data!=nullptr && ret == ESP_OK;
        }

        void end() override {
            dsps_fft2r_deinit_fc32();
            if (p_data==nullptr){
                delete p_data;
                p_data = nullptr;
            }
        }

        void setValue(int idx, int value) override {
            if (idx<N){
                p_data[idx*2 + 0] = value;
                p_data[idx*2 + 1] = 0.0f;
            }
        }

        void fft() override {
            ret = dsps_fft2r_fc32(p_data, N);
            if (ret  != ESP_OK){
                LOGE("dsps_fft2r_fc32 %d", ret);
            }
            // Bit reverse 
            ret = dsps_bit_rev_fc32(p_data, N);
            if (ret  != ESP_OK){
                LOGE("dsps_bit_rev_fc32 %d", ret);
            }
            // Convert one complex vector to two complex vectors
            ret = dsps_cplx2reC_fc32(p_data, N);
            if (ret  != ESP_OK){
                LOGE("dsps_cplx2reC_fc32 %d", ret);
            }
        };

        float magnitude(int idx) override { 
            return sqrt(p_data[idx*2] * p_data[idx*2] + p_data[idx*2+1] * p_data[idx*2+1]);
        }

        /// magnitude w/o sqrt
        float magnitudeFast(int idx) override { 
            return (p_data[idx*2] * p_data[idx*2] + p_data[idx*2+1] * p_data[idx*2+1]);
        }

        virtual bool isValid() override{ return p_data!=nullptr && ret==ESP_OK; }

        esp_err_t ret;
        float *p_data = nullptr;
        int N=0;

};
/**
 * @brief AudioFFT using FFTReal. The only specific functionality is the access to the dataArray
 * @ingroup fft-dsp
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioEspressifFFT : public AudioFFTBase {
    public:
        AudioEspressifFFT():AudioFFTBase(new FFTDriverEspressifFFT()) {}

        /// Provides the complex array returned by the FFT  
        float *dataArray() {
            return driverEx()->p_data;
        }

        FFTDriverEspressifFFT* driverEx() {
            return (FFTDriverEspressifFFT*)driver();
        }
};


}