#pragma once

#include "AudioTools/AudioOutput.h"
#include "AudioLibs/FFT/FFTWindows.h"

namespace audio_tools {

// forward declaration
class AudioFFTBase;
MusicalNotes AudioFFTNotes;

/// Result of the FFT
struct AudioFFTResult {
    int bin;
    float magnitude;
    float frequency;

    int frequncyAsInt(){
        return round(frequency);
    }
    const char* frequencyAsNote() {
        return AudioFFTNotes.note(frequncyAsInt());
    }
    const char* frequencyAsNote(int &diff) {
        return AudioFFTNotes.note(frequncyAsInt(), diff);
    }
};

/// Configuration for AudioFFT
struct AudioFFTConfig : public  AudioBaseInfo {
    AudioFFTConfig(){
        channels = 2;
        bits_per_sample = 16;
        sample_rate = 44100;
    }
    /// Callback method which is called after we got a new result
    void (*callback)(AudioFFTBase &fft) = nullptr;
    /// Channel which is used as input
    uint8_t channel_used = 0; 
    int length=8192;
    int stride=0;
    /// Optional window function
    WindowFunction *window_function = nullptr;  
};

/**
 * @brief Abstract Class which defines the basic FFT functionality 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FFTDriver {
    public:
        virtual void begin(int len) =0;
        virtual void end() =0;
        virtual void setValue(int pos, int value) =0;
        virtual void fft() = 0;
        virtual float magnitude(int idx) = 0;
        virtual bool isValid() = 0;
};

/**
 * @brief Executes FFT using audio data. The Driver which is passed in the constructor selects a specifc FFT implementation
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioFFTBase : public AudioPrint {
    public:
        /// Default Constructor. The len needs to be of the power of 2 (e.g. 512, 1024, 2048, 4096, 8192)
        AudioFFTBase(FFTDriver* driver){
            p_driver = driver;
        }

        ~AudioFFTBase() {
            end();
        }

        /// Provides the default configuration
        AudioFFTConfig defaultConfig() {
            AudioFFTConfig info;
            return info;
        }

        /// starts the processing
        bool begin(AudioFFTConfig info) {
            cfg = info;
            if (!isPowerOfTwo(cfg.length)){
                LOGE("Len must be of the power of 2: %d", cfg.length);
                return false;
            }
            if (!createStrideBuffer()){
                return false;
            }
            p_driver->begin(cfg.length);
            if (cfg.window_function!=nullptr){
                cfg.window_function->begin(cfg.length);
            }

            current_pos = 0;
            return p_driver->isValid();
        }

        /// Notify change of audio information
        void setAudioInfo(AudioBaseInfo info) override {
            cfg.bits_per_sample = info.bits_per_sample;
            cfg.sample_rate = info.sample_rate;
            cfg.channels = info.channels;
            begin(cfg);
        }

        /// Release the allocated memory
        void end() {
            p_driver->end();
            if (p_stridebuffer!=nullptr) delete p_stridebuffer;
            if (p_magnitudes!=nullptr) delete []p_magnitudes;
        }

        /// Provide the audio data as FFT input
        size_t write(const uint8_t*data, size_t len) override {
            size_t result = 0;
            if (p_driver->isValid()){
                result = len;
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
            }
            return result;
        }

        /// We try to fill the buffer at once
        int availableForWrite() {
            return cfg.bits_per_sample/8*cfg.length;
        }

        /// The number of bins used by the FFT which are relevant for the result
        int size() {
            return cfg.length/2;
        }

        /// time when the last result was provided - you can poll this to check if we have a new result
        unsigned long resultTime() {
            return timestamp;
        }

        /// Determines the frequency of the indicated bin
        float frequency(int bin){
            return static_cast<float>(bin) * cfg.sample_rate / cfg.length;
        }

        /// Determines the result values in the max magnitude bin
        AudioFFTResult result() {
            AudioFFTResult ret_value;
            ret_value.magnitude = 0;
            ret_value.bin = 0;
            // find max value and index
            for (int j=1;j<size();j++){
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
            for (int j=1;j<size();j++){
                act.magnitude = magnitude(j);
                act.bin = j;
                act.frequency = frequency(j);
                insertSorted(result, act);
            }
        }

        /// provides access to the FFTDriver which implements the basic FFT functionality
        FFTDriver *driver() {
            return p_driver;
        }

        /// Calculates the magnitude of the fft result to determine the max value (bin is 0 to size())
        float magnitude(int bin){
            return p_driver->magnitude(bin);
        }

        /// Provides the magnitudes as array of size size(). Please note that this method is allocating additinal memory!
        float* magnitudes() {
            if (p_magnitudes==nullptr){
                p_magnitudes = new float[size()];
            }
            for (int j=0;j<size();j++){
                p_magnitudes[j]= magnitude(j);
            }
            return p_magnitudes;
        }

        /// Provides the actual configuration
        AudioFFTConfig config() {
            return cfg;
        }

    protected:
        FFTDriver *p_driver=nullptr;
        int current_pos = 0;
        AudioFFTConfig cfg;
        unsigned long timestamp=0l;
        RingBuffer<uint8_t> *p_stridebuffer = nullptr;
        float *p_magnitudes = nullptr;

        /// Allocates the stride buffer if necessary
        bool createStrideBuffer() {
            bool result = true;
            if (p_stridebuffer!=nullptr) delete p_stridebuffer;
            p_stridebuffer = nullptr;
            if (cfg.stride>0){
                // calculate the number of last bytes that we need to reprocess 
                int size = cfg.length - cfg.stride;
                if (size>0){
                    p_stridebuffer = new RingBuffer<uint8_t>(size*bytesPerSample());
                } else {
                    LOGE("stride>length not supported");
                    result = false;
                }
            }
            return result;
        }


        // Add samples to input data p_x - and process them if full
        template<typename T>
        void processSamples(const void *data, size_t byteCount) {
            T *dataT = (T*) data;
            T sample;
            float sample_windowed;
            int samples = byteCount/sizeof(T);
            for (int j=0; j<samples; j+=cfg.channels){
                sample = dataT[j+cfg.channel_used];
                sample_windowed = sample;
                // optionally apply window function
                if (cfg.window_function!=nullptr){
                    sample_windowed = cfg.window_function->factor(current_pos) * sample;
                }
                p_driver->setValue(current_pos, sample_windowed);
                writeStrideBuffer((uint8_t*)&sample, sizeof(T));
                if (++current_pos>=cfg.length){
                    fft();
                }
            }
        }

        void fft() {
            p_driver->fft();
            timestamp = millis();
            if (cfg.callback!=nullptr){
                cfg.callback(*this);
            }

            // reprocess data in stride buffer
            if (p_stridebuffer!=nullptr){
                // rewrite data in stride buffer
                int byte_count = p_stridebuffer->available();
                uint8_t buffer[byte_count];
                p_stridebuffer->readArray(buffer, byte_count);
                write(buffer, byte_count);
                current_pos = byte_count / bytesPerSample();
            } else {
                current_pos = 0;
            }
        }

        int bytesPerSample() {
            return cfg.bits_per_sample / 8;
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

        void writeStrideBuffer(uint8_t* buffer, size_t len){
            if (p_stridebuffer!=nullptr){
                int available = p_stridebuffer->availableForWrite();
                if (len>available){
                    // clear oldest values to make space
                    int diff = len-available;
                    for(int j=0;j<diff;j++){
                        p_stridebuffer->read();
                    }
                }
                p_stridebuffer->writeArray(buffer, len);
            }
        }

        bool isPowerOfTwo(uint16_t x) {
            return (x & (x - 1)) == 0;
        }

};



}