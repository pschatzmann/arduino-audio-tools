#pragma once

#include "AudioCodecs/AudioEncoded.h"
#include "MP3EncoderLAME.h"

/** 
 * @defgroup codec-lame LAME
 * @ingroup codecs
 * @brief LAME MP3 encoder   
**/

namespace audio_tools {

/**
 * @brief LAME parameters
 * @ingroup codec-lame
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct AudioInfoLAME :  AudioBaseInfo  {
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
 * @ingroup codec-lame
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
    void setOutputStream(Print &out_stream){
        TRACED();
        p_print = &out_stream;
        if (enc!=nullptr){
            enc->setOutput(out_stream);
        }
    }

    /// Defines the Audio Info
    void setAudioInfo(AudioBaseInfo from) {
        TRACED();
        info.channels = from.channels;
        info.sample_rate = from.sample_rate;
        info.bits_per_sample = from.bits_per_sample;
    }

    /// Defines the Audio Info
    void setAudioInfo(AudioInfoLAME from) {
        TRACED();
        info = from;
    }

    // starts the processing
    void begin() {
        createEnc();
        enc->begin();
    }

    /**
     * @brief Opens the encoder  
     * 
     * @param info 
     * @return int 
     */
    void begin(AudioInfoLAME info) {
        TRACED();
        createEnc();
        setAudioInfo(info);
        enc->begin();
    }

    // /**
    //  * @brief Opens the encoder  
    //  * 
    //  * @param input_channels 
    //  * @param input_sample_rate 
    //  * @param input_bits_per_sample 
    //  */
    // void begin(int input_channels, int input_sample_rate, int input_bits_per_sample) {
    //     TRACED();
    //     createEnc();
    //     enc->begin(input_channels, input_sample_rate, input_bits_per_sample);
    // }


    AudioInfoLAME &audioInfo(){
        return info;
    }

    AudioInfoLAME defaultConfig(){
        AudioInfoLAME def;
        return def;
    }

    // convert PCM data to convert into MP3
    size_t write(const void *in_ptr, size_t in_size){
        if (enc==nullptr) return 0;
        LOGD("write %d bytes", (int) in_size);
        return enc->write((uint8_t*)in_ptr, in_size);
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
    AudioInfoLAME info;
    Print *p_print=nullptr;

    // Create enc only at begin so that we can use psram
    void createEnc(){
        if (enc==nullptr){
            enc = new liblame::MP3EncoderLAME();
            if (p_print!=nullptr){
                setOutputStream(*p_print);
            }
            liblame::AudioInfo tmp;
            tmp.channels = info.channels;
            tmp.sample_rate = info.sample_rate;
            tmp.bits_per_sample = info.bits_per_sample;
            tmp.quality = info.quality;
            LOGI("LibLAME channels: %d", tmp.channels);
            LOGI("LibLAME sample_rate: %d", tmp.sample_rate);
            LOGI("LibLAME bits_per_sample: %d", tmp.bits_per_sample);
            LOGI("LibLAME quality: %d", tmp.quality);
            enc->setAudioInfo(tmp);
        }
    }

};

}
