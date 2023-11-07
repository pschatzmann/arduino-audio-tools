#pragma once

#include "AudioConfig.h"
#include "AudioBasic/Str.h"
#include "AudioBasic/Debouncer.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/BaseConverter.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/StreamCopy.h"
#include "AudioHttp/AudioHttp.h"
#include "AudioTools/AudioSource.h"
#include "AudioTools/Fade.h"
// support for legacy USE_SDFAT
#ifdef USE_SDFAT
#include "AudioLibs/AudioSourceSDFAT.h"
#endif

/**
 * @defgroup player Player
 * @ingroup main
 * @brief Audio Player
 */


namespace audio_tools {

    /**
     * @brief Implements a simple audio player which supports the following commands:
     * - begin
     * - play
     * - stop
     * - next
     * - set Volume
     * @ingroup player
     * @author Phil Schatzmann
     * @copyright GPLv3
     */
    class AudioPlayer : public AudioInfoSupport {

    public:

        /// Default constructor 
        AudioPlayer() {
            TRACED();
        }

        /**
         * @brief Construct a new Audio Player object. The processing chain is
         * AudioSource -> Stream-copy -> EncodedAudioStream -> VolumeStream -> FadeStream -> Print
         *
         * @param source
         * @param output
         * @param decoder
         */
        AudioPlayer(AudioSource& source, AudioOutput& output, AudioDecoder& decoder) {
            TRACED();
            this->p_source = &source;
            this->p_decoder = &decoder;
            setOutput(output);
            // notification for audio configuration
            decoder.setNotifyAudioChange(*this);
        }

        /**
         * @brief Construct a new Audio Player object. The processing chain is
         * AudioSource -> Stream-copy -> EncodedAudioStream -> VolumeStream -> FadeStream -> Print
         *
         * @param source
         * @param output
         * @param decoder
         * @param notify
         */
        AudioPlayer(AudioSource& source, Print& output, AudioDecoder& decoder, AudioInfoSupport* notify = nullptr) {
            TRACED();
            this->p_source = &source;
            this->p_decoder = &decoder;
            setOutput(output);
            setNotify(notify);
        }

        /**
         * @brief Construct a new Audio Player object. The processing chain is
         * AudioSource -> Stream-copy -> EncodedAudioStream -> VolumeStream -> FadeStream -> Print
         *
         * @param source
         * @param output
         * @param decoder
         */
        AudioPlayer(AudioSource& source, AudioStream& output, AudioDecoder& decoder) {
            TRACED();
            this->p_source = &source;
            this->p_decoder = &decoder;
            setOutput(output);
            // notification for audio configuration
            decoder.setNotifyAudioChange(*this);
        }

        AudioPlayer(AudioPlayer const&) = delete;

        AudioPlayer& operator=(AudioPlayer const&) = delete;

        void setOutput(AudioOutput& output){
            if (p_decoder->isResultPCM()){
                this->fade.setOutput(output);
                this->volume_out.setOutput(fade);
                out_decoding.setOutput(&volume_out);
                out_decoding.setDecoder(p_decoder);
            } else {
                out_decoding.setOutput(&output);
                out_decoding.setDecoder(p_decoder);
            }
            this->p_final_print = &output;
            this->p_final_stream = nullptr;
        }

        void setOutput(Print &output){
            if (p_decoder->isResultPCM()){
                this->fade.setOutput(output);
                this->volume_out.setOutput(fade);
                out_decoding.setOutput(&volume_out);
                out_decoding.setDecoder(p_decoder);
            } else {
                out_decoding.setOutput(&output);
                out_decoding.setDecoder(p_decoder);
            }
            this->p_final_print = nullptr;
            this->p_final_stream = nullptr;
        }

        void setOutput(AudioStream& output){
            if (p_decoder->isResultPCM()){
                this->fade.setOutput(output);
                this->volume_out.setOutput(fade);
                out_decoding.setOutput(&volume_out);
                out_decoding.setDecoder(p_decoder);
            } else {
                out_decoding.setOutput(&output);
                out_decoding.setDecoder(p_decoder);

            }
            this->p_final_print = nullptr;
            this->p_final_stream = &output;
        }

        /// Defines the number of bytes used by the copier
        virtual void setBufferSize(int size){
            copier.resize(size);
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

            // take definition from source
            autonext = p_source->isAutoNext();

            // initial audio info for fade from output when not defined yet
            setupFade();
            
            // start dependent objects
            out_decoding.begin();
            p_source->begin();
            meta_out.begin();

            if (index >= 0) {
                p_input_stream = p_source->selectStream(index);
                if (p_input_stream != nullptr) {
                    if (meta_active) {
                        copier.setCallbackOnWrite(decodeMetaData, this);
                    }
                    copier.begin(out_decoding, *p_input_stream);
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
            out_decoding.end();
            meta_out.end();
            // remove any data in the decoder
            if (p_decoder!=nullptr){
                LOGI("reset codec");
                p_decoder->end();
                p_decoder->begin();
            }
        }

        /// (Re)defines the audio source
        void setAudioSource(AudioSource& source){
            this->p_source = &source;
        }

        /// (Re)defines the decoder
        void setDecoder(AudioDecoder& decoder){
            this->p_decoder = &decoder;
            out_decoding.setDecoder(p_decoder);
        }

        /// (Re)defines the notify
        void setNotify(AudioInfoSupport* notify){
            this->p_final_notify = notify;
            // notification for audio configuration
            if (p_decoder!=nullptr){
                p_decoder->setNotifyAudioChange(*this);
            }
        } 

        /// Updates the audio info in the related objects
        virtual void setAudioInfo(AudioInfo info) override {
            TRACED();
            LOGI("sample_rate: %d", info.sample_rate);
            LOGI("bits_per_sample: %d", info.bits_per_sample);
            LOGI("channels: %d", info.channels);
            this->info = info;
            // notifiy volume
            volume_out.setAudioInfo(info);
            fade.setAudioInfo(info);
            // notifiy final ouput: e.g. i2s
            if (p_final_print != nullptr) p_final_print->setAudioInfo(info);
            if (p_final_stream != nullptr) p_final_stream->setAudioInfo(info);
            if (p_final_notify != nullptr) p_final_notify->setAudioInfo(info);
        };

        virtual AudioInfo audioInfo() override {
            return info;
        }

        /// starts / resumes the playing after calling stop()
        virtual void play() {
            TRACED();
            setActive(true);
        }

        /// halts the playing
        virtual void stop() {
            TRACED();
            setActive(false);
        }

        /// moves to next file or nth next file when indicating an offset. Negative values are supported to move back.
        virtual bool next(int offset=1) {
            TRACED();
            writeEnd();
            stream_increment = offset >= 0 ? 1 : -1;
            active = setStream(p_source->nextStream(offset));
            return active;
        }

        /// moves to selected file
        virtual bool setIndex(int idx) {
            TRACED();
            writeEnd();
            stream_increment = 1;
            active = setStream(p_source->selectStream(idx));
            return active;
        }

        /// moves to selected file
        virtual bool setPath(const char* path) {
            TRACED();
            writeEnd();
            stream_increment = 1;
            active = setStream(p_source->selectStream(path));
            return active;
        }

        /// moves to previous file
        virtual bool previous(int offset=1) {
            TRACED();
            writeEnd();
            stream_increment = -1;
            active = setStream(p_source->previousStream(abs(offset)));
            return active;
        }

        /// start selected input stream
        virtual bool setStream(Stream *input) {
            end();
            out_decoding.begin();
            p_input_stream = input;
            if (p_input_stream != nullptr) {
                LOGD("open selected stream");
                meta_out.begin();
                copier.begin(out_decoding, *p_input_stream);
            }
            return p_input_stream != nullptr;
        }

        /// Provides the actual stream (=e.g.file)
        virtual Stream* getStream(){
            return p_input_stream;
        }

        /// determines if the player is active
        virtual bool isActive() {
            return active;
        }

        /// determines if the player is active
        operator bool() {
            return isActive();
        }

        /// The same like start() / stop()
        virtual void setActive(bool isActive){
            if (is_auto_fade){
                if (isActive){
                    fade.setFadeInActive(true); 
                } else {
                    fade.setFadeOutActive(true); 
                    copier.copy();
                    writeSilence(2048);
                }            
            }
            active = isActive;
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

        /// Set automatically move to next file and end of current file: This is determined from the AudioSource. If you want to override it call this method after calling begin()!
        virtual void setAutoNext(bool next) {
            autonext = next;
        }

        /// Defines the wait time in ms if the target output is full
        virtual void setDelayIfOutputFull(int delayMs){
            delay_if_full = delayMs;
        }

        /// Call this method in the loop. 
        virtual size_t copy() {
            size_t result = 0;
            if (active) {
                TRACED();
                if (delay_if_full!=0 && ((p_final_print!=nullptr && p_final_print->availableForWrite()==0) || (p_final_stream!=nullptr && p_final_stream->availableForWrite()==0))){
                    // not ready to do anything - so we wait a bit
                    delay(delay_if_full);
                    return 0;
                }
                // handle sound
                result = copier.copy();
                if (result>0 || timeout == 0) {
                    // reset timeout if we had any data
                    timeout = millis() + p_source->timeoutAutoNext();
                }
                // move to next stream after timeout
                moveToNextFileOnTimeout();
            } else {
                // e.g. A2DP should still receive data to keep the connection open
                if (silence_on_inactive){
                    writeSilence(1024);
                }
            }
            return result;
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

        /// Provides access to the StreamCopy, so that we can register additinal callbacks
        StreamCopy &getStreamCopy(){
            return copier;
        }

        /// If set to true the player writes 0 values instead of no data if the player is inactive
        void setSilenceOnInactive(bool active){
            silence_on_inactive = active;
        }

        /// Checks if silence_on_inactive has been activated (default false)
        bool isSilenceOnInactive(){
            return silence_on_inactive;
        }

        void writeSilence(size_t bytes) {
            TRACEI();
            if (p_final_print!=nullptr){
                p_final_print->writeSilence(bytes);
            } else if (p_final_stream!=nullptr){
                p_final_stream->writeSilence(bytes);
            }
        }

        /// Provides the Print object to which we send the decoding result
        Print* getVolumeOutput(){
            return &volume_out;
        }

        /// Activates/deactivates the automatic fade in and fade out to prevent popping sounds: default is active
        void setAutoFade(bool active){
            is_auto_fade = active;
        }

        bool isAutoFade() {
            return is_auto_fade;
        }

    protected:
        bool active = false;
        bool autonext = true;
        bool silence_on_inactive = false;
        AudioSource* p_source = nullptr;
        VolumeStream volume_out; // Volume control
        FadeStream fade; // Phase in / Phase Out to avoid popping noise
        MetaDataID3 meta_out; // Metadata parser
        EncodedAudioOutput out_decoding; // Decoding stream
        CopyDecoder no_decoder{true};
        AudioDecoder* p_decoder = &no_decoder;
        Stream* p_input_stream = nullptr;
        AudioOutput* p_final_print = nullptr;
        AudioStream* p_final_stream = nullptr;
        AudioInfoSupport* p_final_notify = nullptr;
        StreamCopy copier; // copies sound into i2s
        AudioInfo info;
        bool meta_active = false;
        uint32_t timeout = 0;
        int stream_increment = 1; // +1 moves forward; -1 moves backward
        float current_volume = -1.0; // illegal value which will trigger an update
        int delay_if_full = 100;
        bool is_auto_fade = true;

        void setupFade() {
            if (p_final_print !=  nullptr) {
                fade.setAudioInfo(p_final_print->audioInfo());
            } else if (p_final_stream != nullptr){
                fade.setAudioInfo(p_final_stream->audioInfo());
            }                
        }


        virtual void moveToNextFileOnTimeout(){
            if (!autonext) return;
            if (p_final_stream==nullptr) return;
            if (p_final_stream->availableForWrite()==0) return;
            if (p_input_stream == nullptr || millis() > timeout) {
                if (is_auto_fade) fade.setFadeInActive(true);
                if (autonext) {
                    LOGI("-> timeout - moving by %d", stream_increment);
                    // open next stream
                    if (!next(stream_increment)) {
                        LOGD("stream is null");
                    }
                } else {
                    active = false;
                }
                timeout = millis() + p_source->timeoutAutoNext();
            }
        }


        void writeEnd() {
            // end silently
            TRACEI();
            if (is_auto_fade){
                fade.setFadeOutActive(true);
                copier.copy();
                // start by fading in
                fade.setFadeInActive(true);
            }
            // restart the decoder to make sure it does not contain any audio when we continue
            p_decoder->begin();
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
