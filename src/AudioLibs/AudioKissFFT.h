#pragma once

#include "kiss_fft.h"

namespace audio_tools {

// forward declaration
class AudioFFT;

/// Result of the FFT
struct AudioFFTResult {
    int bin;
    float magnitude;
    float frequency;
};

/// Configuration for AudioFFT
struct AudioFFTConfig : public  AudioBaseInfo {
    /// Callback method which is called after we got a new result
    void (*callback)(AudioFFT &fft) = nullptr;
    /// Channel which is used as input
    uint8_t channel_used = 0; 
};


/**
 * @brief Executes FFT using audio data. We use the KissFFT library https://github.com/pschatzmann/kissfft which 
 * needs to be installed in the lib folder of Arduino.
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioFFT : public AudioPrint {
    public:
        /// Default Constructor. The len needs to be of the power of 2 (e.g. 512, 1024, 2048, 4096, 8192)
        AudioFFT(uint16_t fft_len){
            len = fft_len;
        }

        ~AudioFFT() {
            end();
        }

        AudioFFTConfig defaultConfig() {
            AudioFFTConfig info;
            info.channels = 2;
            info.bits_per_sample = 16;
            info.sample_rate = 44100;
            return info;
        }

        /// starts the processing
        bool begin(AudioFFTConfig info) {
            cfg = info;
            if (!isPowerOfTwo(len)){
                LOGE("Len must be of the power of 2: %d", len);
                return false;
            }
            if (p_fft_object==nullptr) p_fft_object = kiss_fft_alloc(len,0,nullptr,nullptr);
            if (p_data==nullptr) p_data = new kiss_fft_cpx[len];
            current_pos = 0;
            return p_data!=nullptr;
        }

        void setAudioInfo(AudioBaseInfo info) override {
            cfg.bits_per_sample = info.bits_per_sample;
            cfg.sample_rate = info.sample_rate;
            cfg.channels = info.channels;
            begin(cfg);
        }

        /// Release the allocated memory
        void end() {
            if (p_fft_object!=nullptr) kiss_fft_free(p_fft_object);
            if (p_data!=nullptr) delete[] p_data;
        }

        /// Provide the audio data as FFT input
        size_t write(const uint8_t*data, size_t len) override {
            size_t result = 0;
            if (p_fft_object!=nullptr){
                result = len;
                switch(cfg.bits_per_sample){
                    case 16:
                        processSamples<int16_t>(data, len);
                        break;
                    default:
                        LOGE("Unsupported bits_per_sample: %d",cfg.bits_per_sample);
                        break;
                }
            }
            return result;
        }

        /// We try to fill the buffer at once
        int availableForWrite() {
            return cfg.bits_per_sample/8*len;
        }


        /// The number of bins used by the FFT 
        int size() {
            return len;
        }

        /// time when the last result was provided - you can poll this to check if we have a new result
        unsigned long resultTime() {
            return timestamp;
        }

        /// Determines the frequency of the indicated bin
        float frequency(int bin){
            return static_cast<float>(bin) * cfg.sample_rate / len;
        }

        /// Determines the result values in the max magnitude bin
        AudioFFTResult result() {
            AudioFFTResult ret_value;
            ret_value.magnitude = 0;
            ret_value.bin = 0;
            // find max value and index
            for (int j=1;j<len/2;j++){
                float m = magnitude(j);
                if (m>ret_value.magnitude){
                    ret_value.magnitude = m;
                    ret_value.bin = j;
                }
            }
            ret_value.frequency = frequency(ret_value.bin);
            return ret_value;
        }


        /// Determines the N biggest result values
        template<int N>
        void resultArray(AudioFFTResult (&result)[N]){
            // initialize to negative value
            for (int j=0;j<N;j++){
                result[j].fft = -1000000;
            }
            // find top n values
            AudioFFTResult act;
            for (int j=1;j<len/2;j++){
                act.magnitude = magnitude(j);
                act.bin = j;
                act.frequency = frequency(j);
                insertSorted(result, act);
            }
        }

    protected:
        kiss_fft_cfg p_fft_object=nullptr;
        kiss_fft_cpx *p_data = nullptr; // real
        int len;
        int current_pos = 0;
        AudioFFTConfig cfg;
        unsigned long timestamp=0l;

        // calculate the magnitued of the fft result to determine the max value
        float magnitude(int idx){
            return sqrt(p_data[idx].r * p_data[idx].r + p_data[idx].i * p_data[idx].i);
        }

        // Add samples to input data p_x - and process them if full
        template<typename T>
        void processSamples(const void *data, size_t byteCount) {
            T *dataT = (T*) data;
            int samples = byteCount/sizeof(T);
            for (int j=0; j<samples; j+=cfg.channels){
                p_data[current_pos].r = dataT[j+cfg.channel_used];
                if (++current_pos>=len){
                    fft();
                }
            }
        }

        void fft() {
            kiss_fft (p_fft_object, p_data, p_data);    
            current_pos = 0;
            timestamp = millis();

            if (cfg.callback!=nullptr){
                cfg.callback(*this);
            }
        }

        /// make sure that we do not reuse already found results
        template<int N>
        bool InsertSorted(AudioFFTResult(&result)[N], AudioFFTResult tmp){
            for (int j=0;j<N;j++){
                if (tmp.magnitude>result[j].magnitude){
                    // shift existing values right
                    for (int i=N-2;i>=j;i--){
                        result[i+1] = result[i];
                    }
                    // insert new value
                    result[j]=tmp;
                }
            }
            return false;
        }

        bool isPowerOfTwo(uint16_t x) {
            return (x & (x - 1)) == 0;
        }

};

}