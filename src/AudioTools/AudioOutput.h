#pragma once
#include "Arduino.h"
#include "AudioConfig.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/int24.h"

#define MAX_SINGLE_CHARS 8

namespace audio_tools {

/**
 * @brief Abstract Audio Ouptut class
 * 
 */
class AudioPrint : public Print {
    public:
        virtual size_t write(const uint8_t *buffer, size_t size) = 0;

        virtual size_t write(uint8_t ch) {
            tmp[tmpPos++] = ch;
            if (tmpPos>MAX_SINGLE_CHARS){
                flush();
            } 
            return 1;
        }

        void flush() {
            write((const uint8_t*)tmp, tmpPos-1);
            tmpPos=0;
        }

    protected:
        uint8_t tmp[MAX_SINGLE_CHARS];
        int tmpPos=0;

};

/**
 * @brief Stream Wrapper which can be used to print the values as readable ASCII to the screen to be analyzed in the Serial Plotter
 * The frames are separated by a new line. The channels in one frame are separated by a ,
 * @tparam T 
  * @author Phil Schatzmann
 * @copyright GPLv3
*/
template<typename T>
class CsvStream : public AudioPrint, public AudioBaseInfoDependent  {

    public:
        CsvStream(int buffer_size=DEFAULT_BUFFER_SIZE, bool active=true) {
            this->active = active;
        }

        /// Constructor
        CsvStream(Print &out, int channels, int buffer_size=DEFAULT_BUFFER_SIZE, bool active=true) {
            this->channels = channels;
            this->out_ptr = &out;
            this->active = active;
        }

        void begin(){
	 		LOGD(LOG_METHOD);
            this->active = true;
        }

        void begin(AudioBaseInfo info){
	 		LOGD(LOG_METHOD);
            this->active = true;
            this->channels = info.channels;
        }

        void begin(int channels, Print &out=Serial){
	 		LOGD(LOG_METHOD);
            this->channels = channels;
            this->out_ptr = &out;
            this->active = true;
        }

        /// Sets the CsvStream as inactive 
        void end() {
	 		LOGD(LOG_METHOD);
            active = false;
        }

        /// defines the number of channels
        virtual void setAudioInfo(AudioBaseInfo info) {
	 		LOGD(LOG_METHOD);
            this->channels = info.channels;
        };

        virtual size_t write(const uint8_t* data, size_t len) {   
            if (!active) return 0;
            size_t lenChannels = len / (sizeof(T)*channels); 
            data_ptr = (T*)data;
            for (int j=0;j<lenChannels;j++){
                for (int ch=0;ch<channels;ch++){
                    out_ptr->print(*data_ptr);
                    data_ptr++;
                    if (ch<channels-1) Serial.print(", ");
                }
                Serial.println();
            }
            return len;
        }

    protected:
        T *data_ptr;
        Print *out_ptr = &Serial;
        int channels = 1;
        bool active = false;

};

/**
 * @brief Creates a Hex Dump
 * 
 */
class HexDumpStream : public AudioPrint {

    public:
        HexDumpStream(int buffer_size=DEFAULT_BUFFER_SIZE, bool active=true) {
            this->active = active;
        }

        /// Constructor
        HexDumpStream(Print &out, int buffer_size=DEFAULT_BUFFER_SIZE, bool active=true) {
            this->out_ptr = &out;
            this->active = active;
        }

        void begin(){
	 		LOGD(LOG_METHOD);
            this->active = true;
            pos = 0;
        }

        /// Sets the CsvStream as inactive 
        void end() {
	 		LOGD(LOG_METHOD);
            active = false;
        }

        void flush(){
            Serial.println();
            pos = 0;
        }

        virtual size_t write(const uint8_t* data, size_t len) {   
            if (!active) return 0;
            for (size_t j=0;j<len;j++){
                out_ptr->print(data[j], HEX);
                out_ptr->print(" ");
                pos++;
                if (pos == 8){
                    Serial.print(" - ");
                }
                if (pos == 16){
                    Serial.println();
                    pos = 0;
                }
            }
            return len;
        }

    protected:
        Print *out_ptr = &Serial;
        int pos = 0;
        bool active = false;
};

/**
 * @brief Construct a new Encoded Stream object which is supporting defined Audio File types
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioOutputStream : public AudioPrint {
    public:
        AudioOutputStream(){
            active = true;
        }        

        AudioOutputStream(AudioWriter &writer){
            decoder_ptr = &writer;
            active = true;
        }
    
        void begin() {
            active = true;
        }

        void end() {
            active = false;
        }

        operator bool() {
            return active && *decoder_ptr;
        }

        virtual size_t write(const uint8_t* data, size_t len) {  
            return (decoder_ptr!=nullptr && active) ? decoder_ptr->write(data, len) : 0;
        }

    protected:
        AudioWriter *decoder_ptr;
        bool active;


};

/**
 * @brief A more natural Stream class to process encoded data (aac, wav, mp3...).
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class EncodedAudioStream : public AudioPrint { 
    public: 
        /**
         * @brief Construct a new Encoded Stream object - used for decoding
         * 
         * @param outputStream 
         * @param decoder 
         */
        EncodedAudioStream(Print &outputStream, AudioDecoder &decoder) {
	 		LOGD(LOG_METHOD);
            decoder_ptr = &decoder;
            decoder_ptr->setOutputStream(outputStream);
            writer_ptr = decoder_ptr;
            active = false;
        }

        /**
         * @brief Construct a new Encoded Audio Stream object - used for decoding
         * 
         * @param outputStream 
         * @param decoder 
         */
        EncodedAudioStream(Print &outputStream, AudioDecoder *decoder) {
	 		LOGD(LOG_METHOD);
            decoder_ptr = decoder;
            decoder_ptr->setOutputStream(outputStream);
            writer_ptr = decoder_ptr;
            active = false;
        }

        /**
         * @brief Construct a new Encoded Audio Stream object - used for decoding
         * 
         * @param outputStream 
         * @param decoder 
         */
        EncodedAudioStream(Print *outputStream, AudioDecoder *decoder) {
	 		LOGD(LOG_METHOD);
            decoder_ptr = decoder;
            decoder_ptr->setOutputStream(*outputStream);
            writer_ptr = decoder_ptr;
            active = false;
        }

        /**
         * @brief Construct a new Encoded Audio Stream object - used for encoding
         * 
         * @param outputStream 
         * @param encoder 
         */
        EncodedAudioStream(Print &outputStream, AudioEncoder &encoder) {
	 		LOGD(LOG_METHOD);
            encoder_ptr = &encoder;
            encoder_ptr->setOutputStream(outputStream);
            writer_ptr = encoder_ptr;
            active = false;
        }

        /**
         * @brief Construct a new Encoded Audio Stream object - used for encoding
         * 
         * @param outputStream 
         * @param encoder 
         */
        EncodedAudioStream(Print &outputStream, AudioEncoder *encoder) {
	 		LOGD(LOG_METHOD);
            encoder_ptr = encoder;
            encoder_ptr->setOutputStream(outputStream);
            writer_ptr = encoder_ptr;
            active = false;
        }

        /**
         * @brief Construct a new Encoded Audio Stream object - used for encoding
         * 
         * @param outputStream 
         * @param encoder 
         */
        EncodedAudioStream(Print *outputStream, AudioEncoder *encoder) {
	 		LOGD(LOG_METHOD);
            encoder_ptr = encoder;
            encoder_ptr->setOutputStream(*outputStream);
            writer_ptr = encoder_ptr;
            active = false;
        }
        /**
         * @brief Destroy the Encoded Audio Stream object
         * 
         */
        ~EncodedAudioStream(){
            if (write_buffer!=nullptr){
                delete [] write_buffer;
            }
        }

        /// Define object which need to be notified if the basinfo is changing
        void setNotifyAudioChange(AudioBaseInfoDependent &bi) {
	 		LOGD(LOG_METHOD);
            decoder_ptr->setNotifyAudioChange(bi);
        }

        /// Starts the processing - sets the status to active
        void begin() {
	 		LOGD(LOG_METHOD);
            decoder_ptr->begin();
            encoder_ptr->begin();
            active = true;
        }

        /// Ends the processing
        void end() {
	 		LOGD(LOG_METHOD);
            decoder_ptr->end();
            encoder_ptr->end();
            active = false;
        }
        
        /// encode the data
        virtual size_t write(const uint8_t *data, size_t len){
	 		LOGD(LOG_METHOD);
            if(writer_ptr==nullptr){
                LOGE("writer_ptr is null");
            }
            return writer_ptr!=nullptr ? writer_ptr->write(data,len) : 0;
        }
        

        /// Returns true if status is active and we still have data to be processed
        operator bool() {
            return active;
        }

        /// Provides the initialized decoder
        AudioDecoder &decoder() {
            return *decoder_ptr;
        }

        /// Provides the initialized encoder
        AudioEncoder &encoder() {
            return *encoder_ptr;
        }

    protected:
        //ExternalBufferStream ext_buffer; 
        AudioDecoder *decoder_ptr = CodecNOP::instance();  // decoder
        AudioEncoder *encoder_ptr = CodecNOP::instance();  // decoder
        AudioWriter *writer_ptr = nullptr ;

        Stream *input_ptr; // data source for encoded data
        uint8_t *write_buffer = nullptr;
        int write_buffer_pos = 0;
        const int write_buffer_size = 256;
        bool active;        
};




/**
 * @brief Output PWM object on which we can apply some volume settings.
 * 
 */
class VolumeOutput : public AudioPrint, public AudioBaseInfoDependent {
    public:
        void begin(Print &out){
            p_out = &out;
        }
        virtual size_t write(const uint8_t *buffer, size_t size){
            applyVolume(buffer,size);
            return p_out->write(buffer, size);
        }

        virtual int availableForWrite() { 
            return p_out==nullptr? 0 : p_out->availableForWrite();
        }

        void setAudioInfo(AudioBaseInfo info){
            this->info = info;
        }

        /// needs to be in the range of 0 to 1.0
        void setVolume(float vol){
            if (vol>1.0) vol=1;
            if (vol<0.0) vol = 0;
            volume_value = vol;
        }

        float volume() {
            return volume_value;
        }

    protected:
        Print *p_out=nullptr;
        AudioBaseInfo info;
        float volume_value=1.0;

        void applyVolume(const uint8_t *buffer, size_t size){
            switch(info.bits_per_sample){
                case 16:
                    applyVolume16((int16_t*)buffer, size/2);
                    break;
                // case 24:
                //     applyVolume24((int24_t*)buffer, size/3);
                //     break;
                case 32:
                    applyVolume32((int32_t*)buffer, size/4);
                    break;
                default:
                    LOGE("Unsupported bits_per_sample: %d", info.bits_per_sample);
            }
        }

        void applyVolume16(int16_t* data, size_t size){
            for (size_t j=0;j<size;j++){
                data[j]= static_cast<int16_t>(volume_value * data[j]);
            }
        }

        // void applyVolume24(int24_t* data, size_t size) {
        //     for (size_t j=0;j<size;j++){
        //         int32_t v = static_cast<int32_t>(data[j]);
        //         data[j]= static_cast<int24_t>(volume_value * v);
        //     }
        // }

        void applyVolume32(int32_t* data, size_t size) {
            for (size_t j=0;j<size;j++){
                data[j]= static_cast<int32_t>(volume_value * data[j]);
            }
        }

};

} //n namespace