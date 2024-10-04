#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"


namespace audio_tools {

/**
 * @brief DecoderFloat - Converts Stream of floats into 2 byte integers
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class DecoderFloat : public AudioDecoder {
    public:
        /**
         * @brief Construct a new DecoderFloat object
         * 
         * @param out_stream Output Stream to which we write the decoded result
         */
        DecoderFloat(Print &out_stream, bool active=true){
            TRACED();
            p_print = &out_stream;
        }

        /**
         * @brief Construct a new DecoderFloat object
         * 
         * @param out_stream Output Stream to which we write the decoded result
         * @param bi Object that will be notified about the Audio Formt (Changes)
         */

        DecoderFloat(Print &out_stream, AudioInfoSupport &bi){
            TRACED();
            p_print = &out_stream;
            addNotifyAudioChange(bi);
        }

        /// Defines the output Stream
        void setOutput(Print &out_stream) override {
            p_print = &out_stream;
        }

        /// Converts data from float to int16_t
        virtual size_t write(const uint8_t *data, size_t len) override {
            if (p_print==nullptr)  return 0;
            int samples = len/sizeof(float);
            buffer.resize(samples);
            float* p_float = (float*) data;
            for (int j=0;j<samples;j++){
                buffer[j] = p_float[j]*32767;
            }
            return p_print->write((uint8_t*)buffer.data(), samples*sizeof(int16_t));
        }

        virtual operator bool() override {
            return p_print!=nullptr;;
        }

    protected:
        Print *p_print=nullptr;
        Vector<int16_t> buffer;

};

/**
 * @brief EncoderFloats - Encodes 16 bit PCM data stream to floats
 * data. 
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class EncoderFloat : public AudioEncoder {
    public: 
        // Empty Constructor - the output stream must be provided with begin()
        EncoderFloat(){
        }        

        // Constructor providing the output stream
        EncoderFloat(Print &out){
            p_print = &out;
        }

        /// Defines the output Stream
        void setOutput(Print &out_stream) override {
            p_print = &out_stream;
        }

        /// Provides "audio/pcm"
        const char* mime() override{
            return mime_pcm;
        }

        /// starts the processing using the actual RAWAudioInfo
        virtual bool begin() override{
            is_open = true;
            return true;
        }

        /// starts the processing
        bool begin(Print &out) {
            p_print = &out;
            return begin();
        }

        /// stops the processing
        void end() override {
            is_open = false;
        }

        /// Converts data from int16_t to float
        virtual size_t write(const uint8_t *data, size_t len) override {
            if (p_print==nullptr)  return 0;
            int16_t *pt16 = (int16_t*)data;
            size_t samples = len / sizeof(int16_t);
            buffer.resize(samples);
            for (size_t j=0;j<samples;j++){
                buffer[j] = static_cast<float>(pt16[j]) / 32768.0;
            }
            return p_print->write((uint8_t*)buffer.data(), samples*sizeof(float));
        }

        operator bool() override {
            return is_open;
        }

        bool isOpen(){
            return is_open;
        }

    protected:
        Print* p_print=nullptr;;
        volatile bool is_open;
        Vector<float> buffer;
        

};

}
