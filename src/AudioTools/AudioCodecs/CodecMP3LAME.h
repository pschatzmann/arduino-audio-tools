#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/HeaderParserMP3.h"
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
 * This is basically just a wrapper using https://github.com/pschatzmann/codec-lame
 *
 * LAME delivers its output in arbitrary chunks (zero, one or several MP3
 * frames per write()). Pass splitFrames = true to the constructor to get
 * exactly one complete MP3 frame per write() to the output instead - as e.g.
 * the video muxers (MuxerMTS, MuxerAVI, ...) expect it.
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MP3EncoderLAME : public AudioEncoder {

public:
    /// @param splitFrames true: write exactly one complete MP3 frame per write()
    MP3EncoderLAME(bool splitFrames = false){
        TRACED();
        split_frames = splitFrames;
    }

    /// @param splitFrames true: write exactly one complete MP3 frame per write()
    MP3EncoderLAME(Print &out_stream, bool splitFrames = false){
        TRACED();
        p_print = &out_stream;
        split_frames = splitFrames;
    }

    ~MP3EncoderLAME(){
        TRACED();
        end();
     }

    /// Defines the output stream
    void setOutput(Print &out_stream){
        TRACED();
        p_print = &out_stream;
        if (split_frames) frames.setOutput(out_stream);
        if (enc!=nullptr){
            if (split_frames) {
                enc->setOutput(frame_writer);
            } else {
                enc->setOutput(out_stream);
            }
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
        if (split_frames) {
            frames.resize(kMaxFrameSize);
            frames.reset();
        }
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
            if (split_frames) frames.flush();
            delete enc;
            enc = nullptr;
        }
    }

    liblame::MP3EncoderLAME *driver() {
        return enc;
    }

    const char *mime() {
        return "audio/mpeg";
    }

    virtual operator bool() {
        return enc!=nullptr && (bool)(*enc);
    }

protected:
    /// Receives the output of LAME and passes it on in complete MP3 frames
    class FrameWriter : public Print {
    public:
        FrameWriter(HeaderParserMP3 &frames) : frames(frames) {}
        size_t write(uint8_t c) override { return write(&c, 1); }
        size_t write(const uint8_t *data, size_t len) override {
            frames.write(data, len);
            frames.flush();  // writes all complete frames
            return len;
        }
    protected:
        HeaderParserMP3 &frames;
    };

    /// Buffer size for the frame splitting: bigger than the largest MP3 frame
    static constexpr int kMaxFrameSize = 2048;

    liblame::MP3EncoderLAME *enc=nullptr;
    AudioInfoLAME lame_info;
    Print *p_print=nullptr;
    bool split_frames = false;
    HeaderParserMP3 frames;
    FrameWriter frame_writer{frames};

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
