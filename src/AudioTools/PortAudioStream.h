#pragma once
/**
 * @brief Generic Implementation of sound input and output for desktop environments using portaudio
 * 
 */

#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)

#include "AudioTools.h"
#include "portaudio.h"

namespace audio_tools {

/**
 * @brief PortAudio information
 * 
 */
class PortAudioConfig : public AudioBaseInfo {
    public:
        PortAudioConfig() = default;
        PortAudioConfig(const PortAudioConfig&) = default;
        PortAudioConfig(const AudioBaseInfo& in) {
            sample_rate = in.sample_rate;
            channels = in.channels;
            bits_per_sample = in.bits_per_sample;
        }

        bool is_input = false;
        bool is_output = true;
};

/**
 * @brief Arduino Audio Stream using PortAudio
 * 
 */
class PortAudioStream : public BufferedStream  :  public AudioBaseInfoDependent {
    public:
        PortAudioStream(int buffer_size=DEFAULT_BUFFER_SIZE):BufferedStream(buffer_size) {
            LOGD(__FUNCTION__);
            this->buffer_size = buffer_size;
        }

        ~PortAudioStream(){
            LOGD(__FUNCTION__);
            Pa_Terminate();
        }

        PortAudioConfig defaultConfig() {
            LOGD(__FUNCTION__);
            PortAudioConfig default_info;
            return default_info;
        }

        /// notification of audio info change
        virtual void setAudioInfo(AudioBaseInfo in) {
            LOGD(__FUNCTION__);
            info.channels = in.channels;
            info.sample_rate = in.sample_rate;
            info.bits_per_sample = in.bits_per_sample;
            begin(info)
        };

        // start with default configuration
        void begin() {
            begin(defaultConfig());
        }

        // start with the indicated configuration
        void begin(PortAudioConfig info) {
            LOGD(__FUNCTION__);
            this->info = info;

            if (info.channels>0 && info.sample_rate && info.bits_per_sample>0){
                LOGD("Pa_Initialize");
                err = Pa_Initialize();
                LOGD("Pa_Initialize - done");
                if( err != paNoError ) {
                    LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
                    return;
                }

                // calculate frames
                int bytes = info.bits_per_sample / 8;
                int buffer_frames = buffer_size / bytes / info.channels;

                // Open an audio I/O stream. 
                LOGD("Pa_OpenDefaultStream");
                err = Pa_OpenDefaultStream( &stream,
                    info.is_input ? info.channels : 0,    // no input channels 
                    info.is_output ? info.channels : 0,   // stereo output 
                    getFormat(info.bits_per_sample),      // format  
                    info.sample_rate,                     // sample rate
                    buffer_frames,                        // frames per buffer 
                    nullptr,   
                    nullptr ); 
                LOGD("Pa_OpenDefaultStream - done");
                if( err != paNoError ) {
                    LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
                }
            } else {
                LOGI("basic audio information is missing...");
            }

        }

        void end() {
            LOGD(__FUNCTION__);
            err = Pa_StopStream( stream );
            if( err != paNoError ) {
                LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
            }

            err = Pa_CloseStream( stream );
            if( err != paNoError ) {
                LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
            }
            stream_started = false;
        }

        operator boolean() {
            return err == paNoError;
        }

    protected:
        PaStream *stream = nullptr;
        PaError err = paNoError;
        PortAudioConfig info;
        bool stream_started = false;
        int buffer_size;

        virtual size_t writeExt(const uint8_t* data, size_t len) {  
            LOGD("writeExt: %zu", len);

            startStream();

            size_t result = 0;
            if (stream!=nullptr){
                int bytes = info.bits_per_sample / 8;
                int frames = len / bytes / info.channels;
                err = Pa_WriteStream( stream, data, frames);
                if( err == paNoError ) {
                    result = len;
                } else {
                    LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
                }
            } else {
                LOGW("stream is null")
            }
            return len;
        }

        virtual size_t readExt( uint8_t *data, size_t len) { 
            LOGD("writeExt: %zu", len);
            size_t result = 0;
            if (stream!=nullptr){
                int bytes = info.bits_per_sample / 8;
                int frames = len / bytes / info.channels;
                err = Pa_ReadStream( stream, data, frames );
                if( err == paNoError ) {
                    result = len;
                } else {
                    LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
                }
            } else {
                LOGW("stream is null")
            }
            return len;            
        }

        PaSampleFormat getFormat(int bitLength){
            switch(bitLength){
                case 8:
                    return paInt8;
                case 16:
                    return paInt16;
                case 24:
                    return paInt24;
                case 32:
                    return paInt32;
            }
            // make sure that we return a valid value 
            return paInt16;
        }

        /// automatically start the stream when we start to get data
        void startStream() {
            if (!stream_started) {
                LOGD(__FUNCTION__);
                err = Pa_StartStream( stream );
                if( err == paNoError ) {
                    stream_started = true;
                } else {
                    stream_started = false;
                    LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
                }
            }            
        }
};

} // namespace

#endif


