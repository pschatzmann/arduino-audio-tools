#pragma once
#include "AudioTools/AudioStreams.h"
#include "AudioLibs/AudioFaustDSP.h"

namespace audio_tools {

/**
 * @brief Integration into Faust DSP see https://faust.grame.fr/
 * To generate code from faust, select src and cpp
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FaustStream : public AudioStreamX {
  public:
    /// Constructor for Faust as Audio Source
    FaustStream(dsp &dsp, bool useSeparateOutputBuffer=true) {
        p_dsp = &dsp;
        p_dsp->buildUserInterface(&ui);
        with_output_buffer = useSeparateOutputBuffer;
    }

    /// Constructor for Faust as Singal Processor - changing an input signal and sending it to out
    FaustStream(dsp &dsp, AudioStream &out, bool useSeparateOutputBuffer=true){
        p_out = &out;
        p_dsp = &dsp;
        p_dsp->buildUserInterface(&ui);
        with_output_buffer = useSeparateOutputBuffer;
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

    /// Determines the value of a parameter
    virtual FAUSTFLOAT getLabelValue(const char*label) {
        return ui.getValue(label);
    }

    /// Defines the value of a parameter 
    virtual bool setLabelValue(const char*label, FAUSTFLOAT value){
        bool result        if (!is_read && !is_write) LOGE("setLabelValue must be called after begin");
 = ui.setValue(label, value);
        LOGI("setLabelValue('%s',%f) -> %s", label, value, result?"true":"false");
        return result;
    }

    /// Checks the parameters and starts the processing
    bool begin(AudioBaseInfo cfg){
        LOGD(LOG_METHOD);
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
        if (with_output_buffer && p_buffer_out==nullptr){
            p_buffer_out = new float*[cfg.channels]();
        }

        LOGI("is_read: %s", is_read?"true":"false");
        LOGI("is_write: %s", is_write?"true":"false");

        return result;
    }

    /// Ends the processing
    void end() {
        LOGD(LOG_METHOD);
        is_read = false;
        is_write = false;
        p_dsp->instanceClear();
    }

    /// Used if FaustStream is used as audio source 
    size_t readBytes(uint8_t *data, size_t len) override {
        size_t result = 0;
        if (is_read){
            LOGD(LOG_METHOD);
            result = len;
            int samples = len / bytes_per_sample;
            allocateFloatBuffer(samples, false);
            p_dsp->compute(samples, nullptr, p_buffer);
            // convert from float to int16
            convertFloatBufferToInt16(samples, data, p_buffer);
        }
        return result;
    }

    /// Used if FaustStream is used as audio sink or filter
    size_t write(const uint8_t *write_data, size_t len) override {
        size_t result = 0;
        if (is_write){
            LOGD(LOG_METHOD);
            int samples = len / bytes_per_sample;
            allocateFloatBuffer(samples, with_output_buffer);
            int16_t *data16 = (int16_t*) write_data;
            // convert to float
            for(int j=0;j<samples;j+=cfg.channels){
                for(int i=0;i<cfg.channels;i++){
                    p_buffer[i][j] =  static_cast<float>(data16[j+i])/32767.0;
                }
            }
            float** p_float_out = with_output_buffer ? p_buffer_out : p_buffer; 
            p_dsp->compute(samples, p_buffer, p_float_out);
            // update buffer with data from faust
            convertFloatBufferToInt16(samples,(void*) write_data, p_float_out);
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
    bool is_init = false;
    bool is_read = false;
    bool is_write = false;
    bool with_output_buffer;
    int bytes_per_sample;
    int buffer_allocated;
    dsp *p_dsp = nullptr;
    AudioBaseInfo cfg;
    AudioStream *p_out=nullptr;
    float** p_buffer=nullptr;
    float** p_buffer_out=nullptr;
    UI ui;

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
    void convertFloatBufferToInt16(int samples, void *data, float**p_float_out){
        int16_t *data16 = (int16_t*) data;
        for (int j=0;j<samples;j+=cfg.channels){
            for (int i=0;i<cfg.channels;i++){
                data16[j+i]=p_float_out[i][j]*32767;
            }
        }
    }

    /// Allocate the buffer that is needed by faust
    void allocateFloatBuffer(int samples, bool allocate_out){
        if (samples>buffer_allocated){
            if (p_buffer[0]!=nullptr){
                for (int j=0;j<cfg.channels;j++){
                    delete[]p_buffer[j];
                    p_buffer[j] = nullptr;
                }
            }
            if (p_buffer_out!=nullptr && p_buffer_out[0]!=nullptr){
                for (int j=0;j<cfg.channels;j++){
                    delete[]p_buffer_out[j];
                    p_buffer_out[j] = nullptr;
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
        if (allocate_out){
            if (p_buffer_out[0]==nullptr){
                const int ch = cfg.channels;
                for (int j=0;j<cfg.channels;j++){
                    p_buffer_out[j] = new float[samples];
                }
            }
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
        if (p_buffer_out!=nullptr) {
            for (int j=0;j<cfg.channels;j++){
                if (p_buffer_out[j]!=nullptr) delete p_buffer_out[j];
            }
            delete[] p_buffer_out;
            p_buffer_out = nullptr;
        } 
    }
};

} // namespace


