#pragma once

#include "AudioCodecs/AudioEncoded.h"

/** 
 * @defgroup codecs8bit 8Bit
 * @ingroup codecs
 * @brief Codec for 8 bit data  
**/

namespace audio_tools {

/**
 * @brief Decoder8Bit - Converts an 8 Bit Stream into 16Bits
 * @ingroup codecs8bit
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Decoder8Bit : public AudioDecoder {
    public:
        /**
         * @brief Construct a new Decoder8Bit object
         */

        Decoder8Bit(){
            TRACED();
        }

        /**
         * @brief Construct a new Decoder8Bit object
         * 
         * @param out_stream Output Stream to which we write the decoded result
         */
        Decoder8Bit(Print &out_stream, bool active=true){
            TRACED();
            p_print = &out_stream;
            this->active = active;
        }

        /**
         * @brief Construct a new Decoder8Bit object
         * 
         * @param out_stream Output Stream to which we write the decoded result
         * @param bi Object that will be notified about the Audio Formt (Changes)
         */

        Decoder8Bit(Print &out_stream, AudioBaseInfoDependent &bi){
            TRACED();
            p_print = &out_stream;
        }

        /// Defines the output Stream
        void setOutputStream(Print &out_stream) override {
            p_print = &out_stream;
        }

        void setNotifyAudioChange(AudioBaseInfoDependent &bi) override {
            this->bid = &bi;
        }

        AudioBaseInfo audioInfo() override {
            return cfg;
        }

        void begin(AudioBaseInfo info) {
            TRACED();
            cfg = info;
            if (bid!=nullptr){
                bid->setAudioInfo(cfg);
            }
            active = true;
        }

        void begin() override {
            TRACED();
            active = true;
        }

        void end() override {
            TRACED();
            active = false;
        }

        virtual size_t write(const void *in_ptr, size_t in_size) override {
            if (p_print==nullptr)  return 0;
            buffer.resize(in_size);
            int8_t* pt8 = (int8_t*) in_ptr;
            for (size_t j=0;j<in_size;j++){
                buffer[j] = pt8[j]*258;
            }
            return p_print->write((uint8_t*)buffer.data(), in_size*sizeof(int16_t));
        }

        virtual operator bool() override {
            return active;
        }

    protected:
        Print *p_print=nullptr;
        AudioBaseInfoDependent *bid=nullptr;
        AudioBaseInfo cfg;
        bool active;
        Vector<int16_t> buffer;

};

/**
 * @brief Encoder8Bits - Condenses 16 bit PCM data stream to 8 bits
 * data. 
 * @ingroup codecs8bit
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class Encoder8Bit : public AudioEncoder {
    public: 
        // Empty Constructor - the output stream must be provided with begin()
        Encoder8Bit(){
        }        

        // Constructor providing the output stream
        Encoder8Bit(Print &out){
            p_print = &out;
        }

        /// Defines the output Stream
        void setOutputStream(Print &out_stream) override {
            p_print = &out_stream;
        }

        /// Provides "audio/pcm"
        const char* mime() override{
            return mime_pcm;
        }

        /// We actually do nothing with this 
        virtual void setAudioInfo(AudioBaseInfo from) override {
        }

        /// starts the processing using the actual RAWAudioInfo
        virtual void begin() override{
            is_open = true;
        }

        /// starts the processing
        void begin(Print &out) {
            p_print = &out;
            begin();
        }

        /// stops the processing
        void end() override {
            is_open = false;
        }

        /// Writes PCM data to be encoded as RAW
        virtual size_t write(const void *in_ptr, size_t in_size) override {
            if (p_print==nullptr)  return 0;
            int16_t *pt16 = (int16_t*)in_ptr;
            buffer.resize(in_size);
            size_t samples = in_size/2;
            for (size_t j=0;j<samples;j++){
                buffer[j] = pt16[j] / 258;
            }

            return p_print->write((uint8_t*)buffer.data(), samples);
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
        Vector<int8_t> buffer;
        

};

}
