#pragma once
/**
 * @brief Generic Implementation of sound input and output for desktop environments using portaudio
 * 
 */

#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
#include "AudioTools.h"
#include "portaudio.h"

/**
 * @brief PortAudio information
 * 
 */
class PortAudioInfo : public AudioBaseInfo {
    bool is_input = false;
    bool is_output = true;
};

/**
 * @brief Arduino Audio Stream using PortAudio
 * 
 */
class PortAudioStream :  BufferedStream {
    public:
        PortAudioStream() {
            self = this;
        }

        ~PortAudioStream(){
            Pa_Terminate();
        }

        /// Singleton support to be compatible with the I2S interace
        PortAudioStream instance() {
            static PortAudioStream inst;
            return inst;
        }

        PortAudioInfo getDefaultInfo() {
            PortAudioInfo default_info;
            return default_info;
        }

        void begin() {
            begin(getDefaultInfo());
        }

        void begin(PortAudioInfo info) {
            this->info = info;
            err = Pa_Initialize();
            if( err != paNoError ) {
                LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
                return;
            }

            /* Open an audio I/O stream. */
            err = Pa_OpenDefaultStream( &stream,
                                        info.is_input ? info.channels : 0,          /* no input channels */
                                        info.is_output ? info.channels : 0,          /* stereo output */
                                        getFormat(info.bits_per_sample),  /* 32 bit integer */
                                        info.sample_rate,
                                        paFramesPerBufferUnspecified, /* frames per buffer, i.e. the number
                                                        of sample frames that PortAudio will
                                                        request from the callback. Many apps
                                                        may want to use
                                                        paFramesPerBufferUnspecified, which
                                                        tells PortAudio to pick the best,
                                                        possibly changing, buffer size.*/
                                        nullptr, /* this is your callback function */
                                        nullptr ); /*This is a pointer that will be passed to
                                                        your callback*/
            if( err != paNoError ) {
                LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
            }

        }

        void end() {
            err = Pa_StopStream( stream );
            if( err != paNoError ) {
                LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
            }

            err = Pa_CloseStream( stream );
            if( err != paNoError ) {
                LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
            }
        }

        operator boolean() {
            return err != paNoError;
        }

    protected:
        PaStream *stream = nullptr;
        PaError err = paNoError;
        PortAudioInfo info;


        virtual size_t writeExt(const uint8_t* data, size_t len) {  
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
            };
            return len;
        }

        virtual size_t readExt( uint8_t *data, size_t len) { 
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
            }
            return len            
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


};

#endif


