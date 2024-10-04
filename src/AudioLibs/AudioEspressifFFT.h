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
            this->len = len;
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

        void setValue(int idx, float value) override {
            if (idx<len){
                p_data[idx*2] = value;
                p_data[idx*2 + 1] = 0.0f;
            }
        }

        float getValue(int idx) override { return p_data[idx * 2]; }

        void fft() override {
            ret = dsps_fft2r_fc32(p_data, len);
            if (ret  != ESP_OK){
                LOGE("dsps_fft2r_fc32 %d", ret);
            }
            // Bit reverse 
            ret = dsps_bit_rev_fc32(p_data, len);
            if (ret  != ESP_OK){
                LOGE("dsps_bit_rev_fc32 %d", ret);
            }
            // Convert one complex vector to two complex vectors
            ret = dsps_cplx2reC_fc32(p_data, len);
            if (ret  != ESP_OK){
                LOGE("dsps_cplx2reC_fc32 %d", ret);
            }
        };

        void rfft() override {
            conjugate();
            ret = dsps_fft2r_fc32(p_data, len);
            if (ret  != ESP_OK){
                LOGE("dsps_fft2r_fc32 %d", ret);
            }
            conjugate();
            // Bit reverse 
            ret = dsps_bit_rev_fc32(p_data, len);
            if (ret  != ESP_OK){
                LOGE("dsps_bit_rev_fc32 %d", ret);
            }
            // Convert one complex vector to two complex vectors
            ret = dsps_cplx2reC_fc32(p_data, len);
            if (ret  != ESP_OK){
                LOGE("dsps_cplx2reC_fc32 %d", ret);
            }
        }

        void conjugate(){
            FFTBin bin;
            for (int j=0;j<len;j++){
                getBin(j, bin);
                bin.conjugate();
                setBin(j, bin);
            }
        }

        float magnitude(int idx) override { 
            return sqrt(magnitudeFast(idx));
        }

        /// magnitude w/o sqrt
        float magnitudeFast(int idx) override { 
            return (p_data[idx*2] * p_data[idx*2] + p_data[idx*2+1] * p_data[idx*2+1]);
        }
        bool setBin(int pos, float real, float img) override {
            if (pos>=len) return false;
            p_data[pos*2] = real;
            p_data[pos*2+1] = img;
            return true;
        }
        
        bool setBin(int pos, FFTBin &bin)  { return FFTDriver::setBin(pos, bin);}

        bool getBin(int pos, FFTBin &bin) override { 
            if (pos>=len) return false;
            bin.real = p_data[pos*2];
            bin.img = p_data[pos*2+1];
            return true;
        }

        bool isReverseFFT() override {return true;}

        bool isValid() override{ return p_data!=nullptr && ret==ESP_OK; }

        esp_err_t ret;
        float *p_data = nullptr;
        int len=0;

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