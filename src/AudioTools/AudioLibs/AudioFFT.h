#pragma once

#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/MusicalNotes.h"
#include "AudioTools/AudioLibs/FFT/FFTWindows.h"

/** 
 * @defgroup fft FFT
 * @ingroup dsp
 * @brief Fast Fourier Transform  
**/

namespace audio_tools {

// forward declaration
class AudioFFTBase;
static MusicalNotes AudioFFTNotes;

/**
 * @brief Result of the FFT
 * @ingroup fft
*/
struct AudioFFTResult {
    int bin;
    float magnitude;
    float frequency;

    int frequencyAsInt(){
        return round(frequency);
    }
    const char* frequencyAsNote() {
        return AudioFFTNotes.note(frequency);
    }
    const char* frequencyAsNote(float &diff) {
        return AudioFFTNotes.note(frequency, diff);
    }
};

/**
 * @brief Configuration for AudioFFT. If there are more then 1 channel the 
 * channel_used is defining which channel is used to perform the fft on.
 * @ingroup fft
 */
struct AudioFFTConfig : public  AudioInfo {
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

/// And individual FFT Bin
struct FFTBin {
    float real;
    float img;

    FFTBin() = default;

    FFTBin(float r, float i) {
        real = r;
        img = i;
    }

    void multiply(float f){
        real *= f;
        img *= f;
    }
   
    void conjugate(){
        img = -img;
    }

    void clear() {
        real = img = 0.0f;
    }
};

/**
 * @brief Abstract Class which defines the basic FFT functionality 
 * @ingroup fft
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FFTDriver {
    public:
        virtual bool begin(int len) =0;
        virtual void end() =0;
        /// Sets the real value
        virtual void setValue(int pos, float value) =0;
        /// Perform FFT
        virtual void fft() = 0;
        /// Calculate the magnitude (fft result) at index (sqr(i² + r²))
        virtual float magnitude(int idx) = 0;
        /// Calculate the magnitude w/o sqare root
        virtual float magnitudeFast(int idx) = 0;
        virtual bool isValid() = 0;
        /// Returns true if reverse FFT is supported
        virtual bool isReverseFFT() {return false;}
        /// Calculate reverse FFT
        virtual void rfft() {LOGE("Not implemented"); }
        /// Get result value from Reverse FFT
        virtual float getValue(int pos) = 0;
        /// sets the value of a bin
        virtual bool setBin(int idx, float real, float img) {return false;}
        /// sets the value of a bin
        bool setBin(int pos, FFTBin &bin) { return setBin(pos, bin.real, bin.img);}
        /// gets the value of a bin
        virtual bool getBin(int pos, FFTBin &bin) { return false;}        
};

/**
 * @brief Executes FFT using audio data. The Driver which is passed in the constructor selects a specifc FFT implementation. 
 * @ingroup fft
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioFFTBase : public AudioOutput {
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
            return begin();
        }

        /// starts the processing
        bool begin() override {
            bins = cfg.length/2;
            if (!isPowerOfTwo(cfg.length)){
                LOGE("Len must be of the power of 2: %d", cfg.length);
                return false;
            }
            if (cfg.stride>0 && cfg.stride<cfg.length){
                // holds last N bytes that need to be reprocessed
                stride_buffer.resize((cfg.length - cfg.stride)*bytesPerSample());
            }
            if (!p_driver->begin(cfg.length)){
                LOGE("Not enough memory");
            }
            if (cfg.window_function!=nullptr){
                cfg.window_function->begin(length());
            }

            current_pos = 0;
            rfft_max = 0;
            return p_driver->isValid();
        }

        /// Just resets the current_pos e.g. to start a new cycle
        void reset(){
            current_pos = 0;
            if (cfg.window_function!=nullptr){
                cfg.window_function->begin(length());
            }
        }

        operator bool() {
            return p_driver!=nullptr && p_driver->isValid();
        }

        /// Notify change of audio information
        void setAudioInfo(AudioInfo info) override {
            cfg.bits_per_sample = info.bits_per_sample;
            cfg.sample_rate = info.sample_rate;
            cfg.channels = info.channels;
            begin(cfg);
        }

        /// Release the allocated memory
        void end() override {
            p_driver->end();
            if (p_magnitudes!=nullptr) delete []p_magnitudes;
        }

        /// Provide the audio data as FFT input
        size_t write(const uint8_t*data, size_t len) override {
            size_t result = 0;
            if (p_driver->isValid()){
                result = len;
                switch(cfg.bits_per_sample){
                    case 16:
                        processSamples<int16_t>(data, len/2);
                        break;
                    case 24:
                        processSamples<int24_t>(data, len/3);
                        break;
                    case 32:
                        processSamples<int32_t>(data, len/4);
                        break;
                    default:
                        LOGE("Unsupported bits_per_sample: %d",cfg.bits_per_sample);
                        break;
                }
            }
            return result;
        }

        /// We try to fill the buffer at once
        int availableForWrite() override {
            return cfg.bits_per_sample/8*cfg.length;
        }

        /// The number of bins used by the FFT which are relevant for the result
        int size() {
            return bins; 
        }

        /// The number of samples
        int length() {
            return cfg.length;
        }

        /// time after the fft: time when the last result was provided - you can poll this to check if we have a new result
        unsigned long resultTime() {
            return timestamp;
        }
        /// time before the fft 
        unsigned long resultTimeBegin() {
            return timestamp_begin;
        }

        /// Determines the frequency of the indicated bin
        float frequency(int bin){
            if (bin>=bins){
                LOGE("Invalid bin %d", bin);
                return 0;
            }
            return static_cast<float>(bin) * cfg.sample_rate / cfg.length;
        }

        /// Determines the result values in the max magnitude bin
        AudioFFTResult result() {
            AudioFFTResult ret_value;
            ret_value.magnitude = 0;
            ret_value.bin = 0;
            // find max value and index
            for (int j=0;j<size();j++){
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
                result[j].magnitude = -1000000;
            }
            // find top n values
            AudioFFTResult act;
            for (int j=0;j<size();j++){
                act.magnitude = magnitude(j);
                act.bin = j;
                act.frequency = frequency(j);
                insertSorted<N>(result, act);
            }
        }

        /// provides access to the FFTDriver which implements the basic FFT functionality
        FFTDriver *driver() {
            return p_driver;
        }

        /// Calculates the magnitude of the fft result to determine the max value (bin is 0 to size())
        float magnitude(int bin){
            if (bin>=bins){
                LOGE("Invalid bin %d", bin);
                return 0;
            }
            return p_driver->magnitude(bin);
        }

        float magnitudeFast(int bin){
            if (bin>=bins){
                LOGE("Invalid bin %d", bin);
                return 0;
            }
            return p_driver->magnitudeFast(bin);
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

        /// Provides the magnitudes w/o calling the square root function as array of size size(). Please note that this method is allocating additinal memory!
        float* magnitudesFast() {
            if (p_magnitudes==nullptr){
                p_magnitudes = new float[size()];
            }
            for (int j=0;j<size();j++){
                p_magnitudes[j]= magnitudeFast(j);
            }
            return p_magnitudes;
        }

        /// sets the value of a bin
        bool setBin(int idx, float real, float img) {return p_driver->setBin(idx, real, img);}
        /// sets the value of a bin
        bool setBin(int pos, FFTBin &bin) { return p_driver->setBin(pos, bin.real, bin.img);}
        /// gets the value of a bin
        bool getBin(int pos, FFTBin &bin) { return p_driver->getBin(pos, bin);}        


        /// Provides the actual configuration
        AudioFFTConfig &config() {
            return cfg;
        }

        /// Define final output for reverse ffft
        void setOutput(Print&out){
            p_out = &out;
        }

        /// Returns true if we need to calculate the inverse FFT
        bool isInverseFFT() {
            return p_out != nullptr && p_driver->isReverseFFT();
        }

    protected:
        FFTDriver *p_driver=nullptr;
        int current_pos = 0;
        AudioFFTConfig cfg;
        unsigned long timestamp_begin=0l;
        unsigned long timestamp=0l;
        RingBuffer<uint8_t> stride_buffer{0};
        float *p_magnitudes = nullptr;
        int bins = 0;
        Print *p_out = nullptr;
        float rfft_max = 0;


        // Add samples to input data p_x - and process them if full
        template<typename T>
        void processSamples(const void *data, size_t samples) {
            T *dataT = (T*) data;
            T sample;
            float sample_windowed;
            for (int j=0; j<samples; j+=cfg.channels){
                sample = dataT[j+cfg.channel_used];
                p_driver->setValue(current_pos, windowedSample(sample));
                writeStrideBuffer((uint8_t*)&sample, sizeof(T));
                if (++current_pos>=cfg.length){
                    // perform FFT
                    fft<T>();

                    if (isInverseFFT())
                        rfft();                    

                    // reprocess data in stride buffer
                    if (stride_buffer.size()>0){
                        // reload data from stride buffer
                        while (stride_buffer.available()){
                            T sample;
                            stride_buffer.readArray((uint8_t*)&sample, sizeof(T));
                            p_driver->setValue(current_pos, windowedSample(sample));
                            current_pos++;
                        }
                    } 

                }
            }
        }

        template<typename T>
        T windowedSample(T sample){
            T result = sample;
            if (cfg.window_function!=nullptr){
                result = cfg.window_function->factor(current_pos) * sample;
            }
            return result;
        }

        template<typename T>
        void fft() {
            timestamp_begin = millis();
            p_driver->fft();
            timestamp = millis();
            if (cfg.callback!=nullptr){
                cfg.callback(*this);
            }
            current_pos = 0;
        }

        /// reverse fft if necessary 
        void rfft() {
            TRACED();
            p_driver->rfft();
            for (int j=0;j<cfg.length;j++){
                float value = p_driver->getValue(j);
                if (rfft_max < value ) rfft_max = value;
                //Serial.println(value / rfft_max);
                switch(cfg.bits_per_sample){
                    case 16:{
                        int16_t out16 = value / rfft_max * NumberConverter::maxValue(16);
                        for (int ch=0;ch<cfg.channels; ch++)
                            p_out->write((uint8_t*)&out16, sizeof(out16));
                        }break;
                    case 24:{
                        int24_t out24 = value  / rfft_max * NumberConverter::maxValue(24);
                        for (int ch=0;ch<cfg.channels; ch++)
                            p_out->write((uint8_t*)&out24, sizeof(out24));
                        }break;
                    case 32: {
                        int32_t out32 = value  / rfft_max * NumberConverter::maxValue(32);
                        for (int ch=0;ch<cfg.channels; ch++)
                            p_out->write((uint8_t*)&out32, sizeof(out32));
                        } break;
                    default:
                        LOGE("Unsupported bits")
                }
            }
        }

        int bytesPerSample() {
            return cfg.bits_per_sample / 8;
        }

        /// make sure that we do not reuse already found results
        template<int N>
        void insertSorted(AudioFFTResult(&result)[N], AudioFFTResult tmp){
            // find place where we need to insert new record
            for (int j=0;j<N;j++){
                // insert when biggen then current record
                if (tmp.magnitude>result[j].magnitude){
                    // shift existing values right
                    for (int i=N-2;i>=j;i--){
                        result[i+1] = result[i];
                    }
                    // insert new value
                    result[j]=tmp;
                    // stop after we found the correct index
                    break;
                }
            }
        }

        void writeStrideBuffer(uint8_t* buffer, size_t len){
            if (stride_buffer.size()>0){
                int available = stride_buffer.availableForWrite();
                if (len>available){
                    // clear oldest values to make space
                    int diff = len-available;
                    for(int j=0;j<diff;j++){
                        stride_buffer.read();
                    }
                }
                stride_buffer.writeArray(buffer, len);
            }
        }

        bool isPowerOfTwo(uint16_t x) {
            return (x & (x - 1)) == 0;
        }

};



}
