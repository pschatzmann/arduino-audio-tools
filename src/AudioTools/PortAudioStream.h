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
        bool is_input = false;
        bool is_output = true;
};

/**
 * @brief Arduino Audio Stream using PortAudio
 * 
 */
class PortAudioStream : public BufferedStream {
    public:
        PortAudioStream():BufferedStream(DEFAULT_BUFFER_SIZE) {
            LOGD(__FUNCTION__);
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

        void begin() {
            begin(defaultConfig());
        }

        void begin(PortAudioConfig info) {
            LOGD(__FUNCTION__);
            this->info = info;
            LOGD("Pa_Initialize");
            err = Pa_Initialize();
            LOGD("Pa_Initialize - done");
            if( err != paNoError ) {
                LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
                return;
            }

            /* Open an audio I/O stream. */
            LOGD("Pa_OpenDefaultStream");
            err = Pa_OpenDefaultStream( &stream,
                info.is_input ? info.channels : 0,          /* no input channels */
                info.is_output ? info.channels : 0,          /* stereo output */
                getFormat(info.bits_per_sample),  
                info.sample_rate,
                paFramesPerBufferUnspecified, /* frames per buffer, i.e. the number
                                of sample frames that PortAudio will
                                request from the callback. Many apps
                                may want to use*/
                nullptr, /* this is your callback function */
                nullptr ); /*This is a pointer that will be passed to
                                your callback*/
            LOGD("Pa_OpenDefaultStream - done");
            if( err != paNoError ) {
                LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
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


