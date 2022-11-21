#pragma once

#include "AudioConfig.h"
#include "AudioBasic/Str.h"
#include "AudioBasic/Debouncer.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/Converter.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioCopy.h"
#include "AudioHttp/AudioHttp.h"
#include "AudioTools/AudioSource.h"
// support for legacy USE_SDFAT
#ifdef USE_SDFAT
#include "AudioLibs/AudioSourceSDFAT.h"
#endif


namespace audio_tools {

    /**
     * @brief Implements a simple audio player which supports the following commands:
     * - begin
     * - play
     * - stop
     * - next
     * - setVolume
     * @author Phil Schatzmann
     * @copyright GPLv3
     */
    class AudioPlayer : public AudioBaseInfoDependent {

    public:
        /**
         * @brief Construct a new Audio Player object. The processing chain is
         * AudioSource -> Stream -copy> EncodedAudioStream -> VolumeOutput -> Print
         *
         * @param source
         * @param output
         * @param decoder
         */
        AudioPlayer(AudioSource& source, AudioPrint& output, AudioDecoder& decoder) {
            TRACED();
            this->p_source = &source;
            this->p_decoder = &decoder;
            if (decoder.isResultPCM()){
                this->volume_out.setTarget(output);
                this->p_out_decoding = new EncodedAudioStream(volume_out, decoder);
            } else {
                this->p_out_decoding = new EncodedAudioStream(output, decoder);
            }
            this->p_final_print = &output;

            // notification for audio configuration
            decoder.setNotifyAudioChange(*this);
        }

        /**
         * @brief Construct a new Audio Player object. The processing chain is
         * AudioSource -> Stream -copy> EncodedAudioStream -> VolumeOutput -> Print
         *
         * @param source
         * @param output
         * @param decoder
         * @param notify
         */
        AudioPlayer(AudioSource& source, Print& output, AudioDecoder& decoder, AudioBaseInfoDependent* notify = nullptr) {
            TRACED();
            this->p_source = &source;
            this->p_decoder = &decoder;
            if (decoder.isResultPCM()){
                this->volume_out.setTarget(output);
                this->p_out_decoding = new EncodedAudioStream(volume_out, decoder);
            } else {
                this->p_out_decoding = new EncodedAudioStream(output, decoder);
            }
            setNotify(notify);
        }

        /**
         * @brief Construct a new Audio Player object. The processing chain is
         * AudioSource -> Stream -copy> EncodedAudioStream -> VolumeOutput -> Print
         *
         * @param source
         * @param output
         * @param decoder
         */
        AudioPlayer(AudioSource& source, AudioStream& output, AudioDecoder& decoder) {
            TRACED();
            this->p_source = &source;
            if (decoder.isResultPCM()){
                this->volume_out.setTarget(output);
                this->p_out_decoding = new EncodedAudioStream(volume_out, decoder);
            } else {
                this->p_out_decoding = new EncodedAudioStream(output, decoder);
            }
            this->p_final_stream = &output;

            // notification for audio configuration
            decoder.setNotifyAudioChange(*this);
        }

        AudioPlayer(AudioPlayer const&) = delete;
        AudioPlayer& operator=(AudioPlayer const&) = delete;

        /// Default destructor
        virtual ~AudioPlayer() {
            if (p_out_decoding != nullptr) {
                delete p_out_decoding;
            }
        }

        /// (Re)Starts the playing of the music (from the beginning)
        virtual bool begin(int index=0, bool isActive = true) {
            TRACED();
            bool result = false;
            // initilaize volume
            if (current_volume==-1){
                setVolume(1.0);
            } else {
                setVolume(current_volume);
            }

            // navigation support
            autonext = p_source->isAutoNext();

            // start dependent objects
            p_out_decoding->begin();
            p_source->begin();
            meta_out.begin();

            if (index >= 0) {
                p_input_stream = p_source->selectStream(index);
                if (p_input_stream != nullptr) {
                    if (meta_active) {
                        copier.setCallbackOnWrite(decodeMetaData, this);
                    }
                    copier.begin(*p_out_decoding, *p_input_stream);
                    timeout = millis() + p_source->timeoutAutoNext();
                    active = isActive;
                    result = true;
                } else {
                    LOGW("-> begin: no data found");
                    active = false;
                    result = false;
                }
            } else {
                LOGW("-> begin: no stream selected");
                active = isActive;
                result = false;
            }
            return result;
        }

        virtual void end() {
            TRACED();
            active = false;
            p_out_decoding->end();
            meta_out.end();
        }

        /// (Re)defines the audio source
        void setAudioSource(AudioSource& source){
            this->p_source = &source;
        }

        /// (Re)defines the output
        void setOutput(Print& output){
            this->volume_out.setTarget(output);
        }

        /// (Re)defines the decoder
        void setDecoder(AudioDecoder& decoder){
            if (this->p_out_decoding!=nullptr){
                delete p_out_decoding;
            }
            this->p_out_decoding = new EncodedAudioStream(volume_out, decoder);
        }

        /// (Re)defines the notify
        void setNotify(AudioBaseInfoDependent* notify){
            this->p_final_notify = notify;
            // notification for audio configuration
            if (p_decoder!=nullptr){
                p_decoder->setNotifyAudioChange(*this);
            }
        } 

        /// Updates the audio info in the related objects
        virtual void setAudioInfo(AudioBaseInfo info) {
            TRACED();
            LOGI("sample_rate: %d", info.sample_rate);
            LOGI("bits_per_sample: %d", info.bits_per_sample);
            LOGI("channels: %d", info.channels);
            // notifiy volume
            volume_out.setAudioInfo(info);
            // notifiy final ouput: e.g. i2s
            if (p_final_print != nullptr) p_final_print->setAudioInfo(info);
            if (p_final_stream != nullptr) p_final_stream->setAudioInfo(info);
            if (p_final_notify != nullptr) p_final_notify->setAudioInfo(info);
        };

        virtual AudioBaseInfo audioInfo() override {
            return volume_out.audioInfo();
        }

        /// starts / resumes the playing of a matching song
        virtual void play() {
            TRACED();
            active = true;
        }

        /// halts the playing
        virtual void stop() {
            TRACED();
            active = false;
        }

        /// moves to next file
        virtual bool next(int offset=1) {
            TRACED();
            previous_stream = false;
            active = setStream(p_source->nextStream(offset));
            return active;
        }

        /// moves to selected file
        virtual bool setIndex(int idx) {
            TRACED();
            previous_stream = false;
            active = setStream(p_source->selectStream(idx));
            return active;
        }

        /// moves to selected file
        virtual bool setPath(const char* path) {
            TRACED();
            previous_stream = false;
            active = setStream(p_source->selectStream(path));
            return active;
        }

        /// moves to previous file
        virtual bool previous(int offset=1) {
            TRACED();
            previous_stream = true;
            active = setStream(p_source->previousStream(offset));
            return active;
        }

        /// start selected input stream
        virtual bool setStream(Stream *input) {
            end();
            p_out_decoding->begin();
            p_input_stream = input;
            if (p_input_stream != nullptr) {
                LOGD("open selected stream");
                meta_out.begin();
                copier.begin(*p_out_decoding, *p_input_stream);
            }
            return p_input_stream != nullptr;
        }

        /// determines if the player is active
        virtual bool isActive() {
            return active;
        }

        /// determines if the player is active
        operator bool() {
            return isActive();
        }

        /// sets the volume - values need to be between 0.0 and 1.0
        virtual void setVolume(float volume) {
            if (volume >= 0.0f && volume <= 1.0f) {
                if (abs(volume - current_volume) > 0.01f) {
                    LOGI("setVolume(%f)", volume);
                    volume_out.setVolume(volume);
                    current_volume = volume;
                }
            } else {
                LOGE("setVolume value '%f' out of range (0.0 -1.0)", volume);
            }
        }

        /// Determines the actual volume
        virtual float volume() {
            return current_volume;
        }

        /// Set move to next
        virtual void setAutoNext(bool next) {
            autonext = next;
        }

        /// Call this method in the loop. 
        virtual void copy() {
            if (active) {
                TRACED();
                if (p_final_print!=nullptr && p_final_print->availableForWrite()==0){
                    // not ready to do anything - so we wait a bit
                    delay(100);
                    return;
                }
                // handle sound
                if (copier.copy() || timeout == 0) {
                    // reset timeout
                    timeout = millis() + p_source->timeoutAutoNext();
                }
                // move to next stream after timeout
                if (p_input_stream == nullptr || millis() > timeout) {
                    if (autonext) {
                        if (previous_stream == false) {
                            LOGW("-> timeout - moving to next stream");
                            // open next stream
                            if (!next(1)) {
                                LOGD("stream is null");
                            }
                        } else {
                            LOGW("-> timeout - moving to previous stream");
                            // open previous stream
                            if (!previous(1)) {
                                LOGD("stream is null");
                            }
                        }
                    } else {
                        active = false;
                    }
                    timeout = millis() + p_source->timeoutAutoNext();
                }
            }
        }

        /// Defines the medatadata callback
        virtual void setMetadataCallback(void (*callback)(MetaDataType type, const char* str, int len), ID3TypeSelection sel=SELECT_ID3) {
            TRACEI();
            // setup metadata. 
            if (p_source->setMetadataCallback(callback)){
                // metadata is handled by source
                LOGI("Using ICY Metadata");
                meta_active = false;
            } else {
                // metadata is handled here
                meta_out.setCallback(callback);
                meta_out.setFilter(sel);
                meta_active = true;
            }
        }

        /// Change the VolumeControl implementation
        virtual void setVolumeControl(VolumeControl& vc) {
            volume_out.setVolumeControl(vc);
        }

    protected:
        bool active = false;
        bool autonext = false;
        AudioSource* p_source = nullptr;
        VolumeStream volume_out; // Volume control
        MetaDataID3 meta_out; // Metadata parser
        EncodedAudioStream* p_out_decoding = nullptr; // Decoding stream
        AudioDecoder* p_decoder = nullptr;
        Stream* p_input_stream = nullptr;
        AudioPrint* p_final_print = nullptr;
        AudioStream* p_final_stream = nullptr;
        AudioBaseInfoDependent* p_final_notify = nullptr;
        StreamCopy copier; // copies sound into i2s
        bool meta_active = false;
        uint32_t timeout = 0;
        bool previous_stream = false;
        float current_volume = -1.0; // illegal value which will trigger an update

        /// Default constructur only allowed in subclasses
        AudioPlayer() {
            TRACED();
        }

        /// Callback implementation which writes to metadata
        static void decodeMetaData(void* obj, void* data, size_t len) {
            LOGD("%s, %zu", LOG_METHOD, len);
            AudioPlayer* p = (AudioPlayer*)obj;
            if (p->meta_active) {
                p->meta_out.write((const uint8_t*)data, len);
            }
        }

    };

}
