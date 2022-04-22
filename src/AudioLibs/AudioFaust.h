#pragma once
#include "AudioTools/AudioStreams.h"
#include "AudioLibs/AudioFaustDSP.h"

namespace audio_tools {

/**
 * @brief Integration into Faust DSP see https://faust.grame.fr/
 * To generate code select src and cpp
 * 
 */
class FaustStream : public AudioStreamX {
  public:
    FaustStream(dsp &dsp) {
        p_dsp = &dsp;
    }

    FaustStream(dsp &dsp, AudioStream &out){
    }

    ~FaustStream(){
        end();
        deleteFloatBuffer();
    }

    AudioBaseInfo defaultConfig() {
        AudioBaseInfo def;
        def.channels = p_dsp->getNumOutputs();
        def.bits_per_sample = 16;
        def.sample_rate = 44100;
        return def;
    }

    bool begin(AudioBaseInfo cfg){
        bool result = true;
        this->cfg = cfg;
        bytes_per_sample = cfg.bits_per_sample/8;
        p_dsp->init(cfg.sample_rate);

        // we do expect an output
        result = checkChannels();

        // allocate array of channel data
        if (p_buffer==nullptr){
            p_buffer = new float*[cfg.channels]();
        }

        return result;
    }

    void end() {
        is_read = false;
        is_write = false;
        p_dsp->instanceClear();
    }

    /// Used if FaustStream is used as audio source 
    size_t readBytes(uint8_t *data, size_t len) override {
        size_t result = 0;
        if (is_read){
            result = len;
            int samples = len / bytes_per_sample;
            allocateFloatBuffer(samples);
            p_dsp->compute(samples, p_buffer, p_buffer);
            // convert from float to int16
            convertFloatBufferToInt16(samples, data);
        }
        return result;
    }

    /// Used if FaustStream is used as audio sink or filter
    size_t write(const uint8_t *write_data, size_t len) override {
        size_t result = 0;
        if (is_write){
            int samples = len / bytes_per_sample;
            allocateFloatBuffer(samples);
            int16_t *data16 = (int16_t*) write_data;
            // convert to float
            for(int j=0;j<samples;j+=cfg.channels){
                for(int i=0;i<cfg.channels;i++){
                    p_buffer[i][j] =  static_cast<float>(data16[j+i])/32767.0;
                }
            }
            p_dsp->compute(samples, p_buffer, p_buffer);
            // update buffer with data from faust
            convertFloatBufferToInt16(samples,(void*) write_data);
            // write data to final output
            result = p_out->write(write_data, len);
        }
        return result;
    }

    int available() override {
        return DEFAULT_BUFFER_SIZE;
    }

    int availableForWrite() override {
        return DEFAULT_BUFFER_SIZE / 4; // we limit the write size to 
    }

  protected:
    bool is_read = false;
    bool is_write = false;
    int bytes_per_sample;
    int buffer_allocated;
    dsp *p_dsp = nullptr;
    AudioBaseInfo cfg;
    AudioStream *p_out=nullptr;
    float** p_buffer=nullptr;

    /// Checks the input and output channels and updates the is_write or is_read scenario flags
    bool checkChannels() {
        bool result = true;
        if (p_dsp->getNumOutputs()>0){
            if (p_dsp->getNumOutputs()==cfg.channels){
                is_read = true;
            } else {
                LOGE("NumOutputs is not matching with number of channels");
                result = false;
            }
            if (p_dsp->getNumInputs()!=0 && p_dsp->getNumInputs()!=cfg.channels){
                LOGE("NumInputs is not matching with number of channels");
                result = false;
            }
            if (p_dsp->getNumInputs()>0){
                if (p_out!=nullptr){
                    is_write = true;   
                } else {
                    LOGE("Faust expects input - you need to provide and AudioStream in the constructor");
                    result = false;
                }
            }
        }
        return result;
    }

    /// Converts the float buffer to int16 values
    void convertFloatBufferToInt16(int samples, void *data){
        int16_t *data16 = (int16_t*) data;
        for (int j=0;j<samples;j+=cfg.channels){
            for (int i=0;i<cfg.channels;i++){
                data16[j+i]=p_buffer[i][j]*32767;
            }
        }
    }

    /// Allocate the buffer that is needed by faust
    void allocateFloatBuffer(int samples){
        if (samples>buffer_allocated){
            if (p_buffer[0]!=nullptr){
                for (int j=0;j<cfg.channels;j++){
                    delete[]p_buffer[j];
                    p_buffer[j] = nullptr;
                }
            }
        }
        if (p_buffer[0]==nullptr){
            const int ch = cfg.channels;
            for (int j=0;j<cfg.channels;j++){
                p_buffer[j] = new float[samples];
            }
            buffer_allocated = samples;
        }
    }

    void deleteFloatBuffer() {
        if (p_buffer!=nullptr) {
            for (int j=0;j<cfg.channels;j++){
                if (p_buffer[j]!=nullptr) delete p_buffer[j];
            }
            delete[] p_buffer;
            p_buffer = nullptr;
        } 
    }
};

} // namespace


