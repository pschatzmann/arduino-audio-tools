#pragma once

#include "FFTReal/FFTReal.h"

namespace audio_tools {

// forward declaration
class AudioFFT;

struct AudioFFTResult {
    int bin;
    float magnitude;
    float frequency;
};

struct AudioFFTConfig : public  AudioBaseInfo {
    void (*callback)(AudioFFT &fft) = nullptr;
};


/**
 * @brief Executes FFT using audio data. We use the FFTReal library see https://github.com/cyrilcode/fft-real
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioFFT : public AudioPrint {
    public:
        /// Default Constructor
        AudioFFT(int fft_len, int channelNo=0){
            len = fft_len;
            channel_no = channelNo;
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
            if (p_fft_object==nullptr) p_fft_object = new ffft::FFTReal<float>(len);
            if (p_x==nullptr) p_x = new float[len];
            if (p_f==nullptr) p_f = new float[len];
            current_pos = 0;
            return p_f!=nullptr;
        }

        void setAudioInfo(AudioBaseInfo info) override {
            cfg.bits_per_sample = info.bits_per_sample;
            cfg.sample_rate = info.sample_rate;
            cfg.channels = info.channels;
            begin(cfg);
        }

        /// Release the allocated memory
        void end() {
            if (p_fft_object!=nullptr) delete p_fft_object;
            if (p_x!=nullptr) delete[] p_x;
            if (p_f!=nullptr) delete[] p_f;
        }

        /// Provide the audio data as FFT input
        size_t write(const uint8_t*data, size_t len) override {
            switch(cfg.bits_per_sample){
                case 16:
                    processSamples<int16_t>(data, len);
                    break;
                case 24:
                    processSamples<int24_t>(data, len);
                    break;
                case 32:
                    processSamples<int32_t>(data, len);
                    break;
                default:
                    LOGE("Unsupported bits_per_sample: %d",cfg.bits_per_sample);
                    break;
            }
            return len;
        }

        /// We try to fill the buffer at once
        int availableForWrite() {
            return cfg.bits_per_sample/8*len;
        }

        /// Provides the raw input array which was used by the FFT to determine the result
        float* dataArray() {
            return p_x;
        }

        /// Provides the raw result array returned by the FFT  
        float *resultArray() {
            return p_f;
        }

        /// The number of bins used by the FFT 
        int size() {
            return len;
        }

        /// Determines the frequency of the indicated bin
        float frequency(int bin){
            return static_cast<float>(bin) * cfg.sample_rate / len;
        }

        /// Determines the result values in the max bin
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

        /// time when the last result was provided - you can poll this to check if we have a new result
        unsigned long resultTime() {
            return timestamp;
        }

        /// Determines the N biggest result values
        template<int N>
        void fftResult(AudioFFTResult (&result)[N]){
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
        ffft::FFTReal <float> *p_fft_object=nullptr;
        float *p_x = nullptr; // input data
        float *p_f = nullptr; // result
        int len;
        int current_pos = 0;
        int channel_no = 0;
        AudioFFTConfig cfg;
        unsigned long timestamp=0l;

        // calculate the magnitued of the fft result to determine the max value
        float magnitude(int idx){
            return sqrt(p_x[idx] * p_x[idx] + p_f[idx] * p_f[idx]);
        }

        // Add samples to input data p_x - and process them if full
        template<typename T>
        void processSamples(const void *data, size_t byteCount) {
            T *dataT = (T*) data;
            int samples = byteCount/sizeof(T);
            for (int j=0; j<samples; j+=cfg.channels){
                p_x[current_pos] = dataT[j+channel_no];
                if (++current_pos>=len){
                    fft();
                }
            }
        }

        void fft() {
            p_fft_object->do_fft (p_f, p_x);    
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

};

}