#pragma once

#include "AudioCodecs/AudioCodecsBase.h"
#include "MP3EncoderLAME.h"

namespace audio_tools {

/**
 * @brief LAME parameters
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct AudioInfoLAME : public liblame::AudioInfo  {
    AudioInfoLAME () {
        sample_rate = 44100;
        channels = 2;
        bits_per_sample = 16;
    };
    AudioInfoLAME (const AudioInfoLAME  &) = default;

    int quality = 7; // 0..9.  0=best (very slow).  9=worst.
};

/**
 * @brief Encodes PCM data to the MP3 format and writes the result to a stream
 * This is basically just a wrapper using https://github.com/pschatzmann/arduino-liblame
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MP3EncoderLAME : public AudioEncoder {

public:
    MP3EncoderLAME(){
        TRACED();
    }

    MP3EncoderLAME(Print &out_stream){
        TRACED();
        p_print = &out_stream;
    }

    ~MP3EncoderLAME(){
        TRACED();
        end();
     }

    /// Defines the output stream
    void setOutput(Print &out_stream){
        TRACED();
        p_print = &out_stream;
        if (enc!=nullptr){
            enc->setOutput(out_stream);
        }
    }

    /// Defines the Audio Info
    void setAudioInfo(AudioInfo from) {
        TRACED();
        AudioEncoder::setAudioInfo(from);
        lame_info.channels = from.channels;
        lame_info.sample_rate = from.sample_rate;
        lame_info.bits_per_sample = from.bits_per_sample;
    }

    /// Defines the Audio Info
    void setAudioInfo(AudioInfoLAME from) {
        TRACED();
        lame_info = from;
    }

    bool begin(AudioInfoLAME from) {
        setAudioInfo(from);
        return begin();
    }

    // starts the processing
    bool begin() {
        createEnc();
        if (enc==nullptr) return false;
        enc->begin();
        return true;
    }


    AudioInfoLAME &audioInfoExt(){
        return lame_info;
    }

    AudioInfoLAME defaultConfig(){
        AudioInfoLAME def;
        return def;
    }

    // convert PCM data to convert into MP3
    size_t write(const uint8_t *data, size_t len){
        if (enc==nullptr) return 0;
        LOGD("write %d bytes", (int) len);
        return enc->write((uint8_t*)data, len);
    }

    // release resources
    void end(){
        TRACED();
        if (enc!=nullptr){
            enc->end();
            delete enc;
            enc = nullptr;
        }
    }

    liblame::MP3EncoderLAME *driver() {
        return enc;
    }

    const char *mime() {
        return "audio/mp3";
    }

    virtual operator bool() {
        return enc!=nullptr && (bool)(*enc);
    }

protected:
    liblame::MP3EncoderLAME *enc=nullptr;
    AudioInfoLAME lame_info;
    Print *p_print=nullptr;

    // Create enc only at begin so that we can use psram
    void createEnc(){
        TRACED();
        if (enc==nullptr){
            enc = new liblame::MP3EncoderLAME();
            if (p_print!=nullptr){
                setOutput(*p_print);
            } else {
                LOGE("Output undefined");
            }
            LOGI("LibLAME channels: %d", lame_info.channels);
            LOGI("LibLAME sample_rate: %d", lame_info.sample_rate);
            LOGI("LibLAME bits_per_sample: %d", lame_info.bits_per_sample);
            LOGI("LibLAME quality: %d", lame_info.quality);
            enc->setAudioInfo(lame_info);
        }
    }

};

}
