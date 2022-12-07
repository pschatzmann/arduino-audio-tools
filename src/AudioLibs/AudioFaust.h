#pragma once
#include "AudioTools/AudioStreams.h"
#include "AudioLibs/AudioFaustDSP.h"

namespace audio_tools {

/**
 * @brief Integration into Faust DSP see https://faust.grame.fr/
 * To generate code from faust, select src and cpp
 * @ingroup dsp
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template<class DSP>
class FaustStream : public AudioStreamX {
  public:

    /// Constructor for Faust as Audio Source
    FaustStream(bool useSeparateOutputBuffer=true) {
        with_output_buffer = useSeparateOutputBuffer;
    }

    /// Constructor for Faust as Singal Processor - changing an input signal and sending it to out
    FaustStream(Print &out, bool useSeparateOutputBuffer=true){
        p_out = &out;
        with_output_buffer = useSeparateOutputBuffer;
    }

    ~FaustStream(){
        end();
        deleteFloatBuffer();
        delete p_dsp;
#ifdef USE_MEMORY_MANAGER
        DSP::classDestroy();
#endif
    }


    /// Provides a pointer to the actual dsp object
    dsp *getDSP(){
        return p_dsp;
    }

    AudioBaseInfo defaultConfig() {
        AudioBaseInfo def;
        def.channels = 2;
        def.bits_per_sample = 16;
        def.sample_rate = 44100;
        return def;
    }


    /// Checks the parameters and starts the processing
    bool begin(AudioBaseInfo cfg){
        TRACED();
        bool result = true;
        this->cfg = cfg;
        this->bytes_per_sample = cfg.bits_per_sample/8;

        if (p_dsp==nullptr){
#ifdef USE_MEMORY_MANAGER
            DSP::fManager = new dsp_memory_manager();
            DSP::memoryInfo();
            p_dsp = DSP::create();
#else
            p_dsp = new DSP();
#endif
        }

        if (p_dsp==nullptr){
            LOGE("dsp is null");
            return false;
        }

        DSP::classInit(cfg.sample_rate);
        p_dsp->buildUserInterface(&ui);
        p_dsp->init(cfg.sample_rate);
        p_dsp->instanceInit(cfg.sample_rate);

        // we do expect an output
        result = checkChannels();

        // allocate array of channel data
        if (p_buffer==nullptr){
            p_buffer = new FAUSTFLOAT*[cfg.channels]();
        }
        if (with_output_buffer && p_buffer_out==nullptr){
            p_buffer_out = new FAUSTFLOAT*[cfg.channels]();
        }

        LOGI("is_read: %s", is_read?"true":"false");
        LOGI("is_write: %s", is_write?"true":"false");
        gate_exists = ui.exists("gate");
        LOGI("gate_exists: %s", gate_exists?"true":"false");

        return result;
    }


    /// Ends the processing
    void end() {
        TRACED();
        is_read = false;
        is_write = false;
        p_dsp->instanceClear();
#ifdef USE_MEMORY_MANAGER
        DSP::destroy(p_dsp);
        p_dsp = nullptr;
#endif

    }

    /// Used if FaustStream is used as audio source 
    size_t readBytes(uint8_t *data, size_t len) override {
        size_t result = 0;
        if (is_read){
            TRACED();
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
            TRACED();
            int samples = len / bytes_per_sample;
            allocateFloatBuffer(samples, with_output_buffer);
            int16_t *data16 = (int16_t*) write_data;
            // convert to float
            int frameCount = samples/cfg.channels;
            for(int j=0;j<frameCount;j++){
                for(int i=0;i<cfg.channels;i++){
                    p_buffer[i][j] =  static_cast<FAUSTFLOAT>(data16[(j*cfg.channels)+i])/32767.0;
                }
            }
            FAUSTFLOAT** p_float_out = with_output_buffer ? p_buffer_out : p_buffer; 
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

    /// Determines the value of a parameter
    virtual FAUSTFLOAT labelValue(const char*label) {
        return ui.getValue(label);
    }

    /// Defines the value of a parameter 
    virtual bool setLabelValue(const char*label, FAUSTFLOAT value){
        if (!is_read && !is_write) LOGE("setLabelValue must be called after begin");
        bool result = ui.setValue(label, value);
        LOGI("setLabelValue('%s',%f) -> %s", label, value, result?"true":"false");
        return result;
    }

    virtual bool setMidiNote(int note){
        FAUSTFLOAT frq = noteToFrequency(note);
        return setFrequency(frq);
    }

    virtual bool setFrequency(FAUSTFLOAT freq){
        return setLabelValue("freq", freq);
    }

    virtual FAUSTFLOAT frequency() {
        return labelValue("freq");
    }

    virtual bool setBend(FAUSTFLOAT bend){
        return setLabelValue("bend", bend);
    }

    virtual FAUSTFLOAT bend() {
        return labelValue("bend");
    }

    virtual bool setGain(FAUSTFLOAT gain){
        return setLabelValue("gain", gain);
    }

    virtual FAUSTFLOAT gain() {
        return labelValue("gain");
    }

    virtual bool midiOn(int note, FAUSTFLOAT gain){
        if (gate_exists) setLabelValue("gate",1.0);
        return setMidiNote(note) && setGain(gain);
    }

    virtual bool midiOff(int note){
        if (gate_exists) setLabelValue("gate",0.0);
        return setMidiNote(note) && setGain(0.0);
    }
    
  protected:
    bool is_init = false;
    bool is_read = false;
    bool is_write = false;
    bool gate_exists = false;
    bool with_output_buffer;
    int bytes_per_sample;
    int buffer_allocated;
    DSP *p_dsp = nullptr;
    AudioBaseInfo cfg;
    Print *p_out=nullptr;
    FAUSTFLOAT** p_buffer=nullptr;
    FAUSTFLOAT** p_buffer_out=nullptr;
    UI ui;

    /// Checks the input and output channels and updates the is_write or is_read scenario flags
    bool checkChannels() {
        bool result = true;

        // update channels
        int num_outputs = p_dsp->getNumOutputs();
        if (cfg.channels!=num_outputs){
            cfg.channels = num_outputs;
            LOGW("Updating channels to %d", num_outputs);
        }

        if (num_outputs>0){
            if (num_outputs==cfg.channels){
                is_read = true;
            } else {
                LOGE("NumOutputs %d is not matching with number of channels %d", num_outputs, cfg.channels);
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
    void convertFloatBufferToInt16(int samples, void *data, FAUSTFLOAT**p_float_out){
        int16_t *data16 = (int16_t*) data;
        int frameCount = samples/cfg.channels;
        for (int j=0;j<frameCount;j++){
            for (int i=0;i<cfg.channels;i++){
                data16[(j*cfg.channels)+i]=p_float_out[i][j]*32767;
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
            for (int j=0;j<ch;j++){
                p_buffer[j] = new FAUSTFLOAT[samples];
            }
            buffer_allocated = samples;
        }
        if (allocate_out){
            if (p_buffer_out[0]==nullptr){
                const int ch = cfg.channels;
                for (int j=0;j<ch;j++){
                    p_buffer_out[j] = new FAUSTFLOAT[samples];
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

    FAUSTFLOAT noteToFrequency(uint8_t x) {
        FAUSTFLOAT note = x;
        return 440.0 * pow(2.0f, (note-69)/12);
    }

};


} // namespace


