#pragma once

#include "AudioTools/AudioTypes.h"

namespace audio_tools {

const char* raw_mime = "audio/pcm";

/**
 * @brief RAWDecoder - Actually this class does no encoding or decoding at all. It just passes on the 
 * data. The reason that this class exists is that we can use the same processing chain for different
 * file types and just replace the decoder.
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RAWDecoder : public AudioDecoder {
    public:
        /**
         * @brief Construct a new RAWDecoder object
         */

        RAWDecoder(){
            LOGD(LOG_METHOD);
        }

        /**
         * @brief Construct a new RAWDecoder object
         * 
         * @param out_stream Output Stream to which we write the decoded result
         */
        RAWDecoder(Print &out_stream, bool active=true){
            LOGD(LOG_METHOD);
            p_print = &out_stream;
            this->active = active;
        }

        /**
         * @brief Construct a new RAWDecoder object
         * 
         * @param out_stream Output Stream to which we write the decoded result
         * @param bi Object that will be notified about the Audio Formt (Changes)
         */

        RAWDecoder(Print &out_stream, AudioBaseInfoDependent &bi){
            LOGD(LOG_METHOD);
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
            LOGD(LOG_METHOD);
            cfg = info;
            if (bid!=nullptr){
                bid->setAudioInfo(cfg);
            }
            active = true;
        }

        void begin() override {
            LOGD(LOG_METHOD);
            active = true;
        }

        void end() override {
            LOGD(LOG_METHOD);
            active = false;
        }

        virtual size_t write(const void *in_ptr, size_t in_size) override {
            if (p_print!=nullptr)  return 0;
            return p_print->write((uint8_t*)in_ptr, in_size);
        }

        virtual operator boolean() override {
            return active;
        }

    protected:
        Print *p_print=nullptr;
        AudioBaseInfoDependent *bid=nullptr;
        AudioBaseInfo cfg;
        bool active;

};

/**
 * @brief RAWDecoder - Actually this class does no encoding or decoding at all. It just passes on the 
 * data. 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RAWEncoder : public AudioEncoder {
    public: 
        // Empty Constructor - the output stream must be provided with begin()
        RAWEncoder(){
        }        

        // Constructor providing the output stream
        RAWEncoder(Print &out){
            p_print = &out;
        }

        /// Defines the output Stream
        void setOutputStream(Print &out_stream) override {
            p_print = &out_stream;
        }

        /// Provides "audio/pcm"
        const char* mime() override{
            return raw_mime;
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
            if (p_print!=nullptr)  return 0;
            return p_print->write((uint8_t*)in_ptr, in_size);
        }

        operator boolean() override {
            return is_open;
        }

        bool isOpen(){
            return is_open;
        }

    protected:
        Print* p_print=nullptr;;
        volatile bool is_open;

};

}
