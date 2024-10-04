#pragma once
/**
 * @brief Generic Implementation of sound input and output for desktop environments using portaudio
 * 
 */

#include "AudioTools.h"
#include "portaudio.h"

namespace audio_tools {

/**
 * @brief PortAudio information
 * 
 */
class PortAudioConfig : public AudioInfo {
    public:
        PortAudioConfig() {
            sample_rate = 44100;
            channels = 2;
            bits_per_sample = 16;
        };
        PortAudioConfig(const PortAudioConfig&) = default;
        PortAudioConfig(const AudioInfo& in) {
            sample_rate = in.sample_rate;
            channels = in.channels;
            bits_per_sample = in.bits_per_sample;
        }

        bool is_input = false;
        bool is_output = true;
};

/**
 * @brief Arduino Audio Stream using PortAudio
 * @ingroup io
 */
class PortAudioStream : public AudioStream {
    public:
        PortAudioStream() {
            TRACED();
        }

        ~PortAudioStream(){
            TRACED();
            Pa_Terminate();
        }

        PortAudioConfig defaultConfig(RxTxMode mode) {
            TRACED();
            PortAudioConfig result;
            switch(mode){
                case RX_MODE:
                    result.is_input = true;
                    result.is_output = false;
                    break;
                case TX_MODE:
                    result.is_input = false;
                    result.is_output = true;
                    break;
                case RXTX_MODE:
                    result.is_input = true;
                    result.is_output = true;
                    break;
                default:
                    LOGE("Unsupported Mode")
                    break;
            }

            return result;
        }
        
        PortAudioConfig defaultConfig() {
            TRACED();
            PortAudioConfig default_info;
            return default_info;
        }

        /// notification of audio info change
        void setAudioInfo(AudioInfo in) override {
            TRACEI();
            info.channels = in.channels;
            info.sample_rate = in.sample_rate;
            info.bits_per_sample = in.bits_per_sample;
            info.logInfo();
            begin(info);
        };

        // start with default configuration
        bool begin() override {
            return begin(defaultConfig());
        }

        // start with the indicated configuration
        bool begin(PortAudioConfig info) {
            TRACED();
            this->info = info;

            if (info.channels>0 && info.sample_rate && info.bits_per_sample>0){
                LOGD("Pa_Initialize");
                err = Pa_Initialize();
                LOGD("Pa_Initialize - done");
                if( err != paNoError ) {
                    LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
                    return false;
                }

                // calculate frames
                int buffer_frames = paFramesPerBufferUnspecified; //buffer_size / bytes / info.channels;

                // Open an audio I/O stream. 
                LOGD("Pa_OpenDefaultStream");
                err = Pa_OpenDefaultStream( &stream,
                    info.is_input ? info.channels : 0,    // no input channels 
                    info.is_output ? info.channels : 0,   // no output 
                    getFormat(),      // format  
                    info.sample_rate,                     // sample rate
                    buffer_frames,                        // frames per buffer 
                    nullptr,   
                    nullptr ); 
                LOGD("Pa_OpenDefaultStream - done");
                if( err != paNoError && err!= paOutputUnderflow ) {
                    LOGE(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
                    return false;
                }
            } else {
                LOGI("basic audio information is missing...");
                return false;
            }
            return true;
        }

        void end() override {
            TRACED();
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

        operator bool() {
            return err == paNoError;
        }

        size_t write(const uint8_t* data, size_t len) override {  
            LOGD("write: %zu", len);

            startStream();

            size_t result = 0;
            if (stream!=nullptr){
                int frames = len / bytesPerSample() / info.channels;
                err = Pa_WriteStream( stream, data, frames);
                if( err == paNoError ) {
                    LOGD("Pa_WriteStream: %zu", len);
                    result = len;
                } else {
                    LOGE("PortAudio error: %s", Pa_GetErrorText( err ) );
                }
            } else {
                LOGW("stream is null")
            }
            return result;
        }

        size_t readBytes( uint8_t *data, size_t len) override { 
            LOGD("readBytes: %zu", len);
            size_t result = 0;
            if (stream!=nullptr){
                int frames = len / bytesPerSample() / info.channels;
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


    protected:
        PaStream *stream = nullptr;
        PaError err = paNoError;
        PortAudioConfig info;
        bool stream_started = false;
        int buffer_size = 10*1024;

        int bytesPerSample(){
            //return info.bits_per_sample / 8;
            return info.bits_per_sample==24 ? sizeof(int24_t): info.bits_per_sample / 8;
        }


        PaSampleFormat getFormat(){
            switch(bytesPerSample()){
                case 1:
                    return paInt8;
                case 2:
                    return paInt16;
                case 3:
                    return paInt24;
                case 4:
                    return paInt32;
            }
            // make sure that we return a valid value 
            return paInt16;
        }

        /// automatically start the stream when we start to get data
        void startStream() {
            if (!stream_started && stream != nullptr) {
                TRACED();
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



