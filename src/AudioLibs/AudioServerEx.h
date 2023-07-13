#pragma once

#include "AudioConfig.h"
#include "AudioTools/AudioOutput.h"
#include "AudioCodecs/CodecWAV.h"
#include "HttpServer.h"
#include "HttpExtensions.h"

namespace audio_tools {

/**
 * @brief Config information for AudioServerEx
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

struct AudioServerExConfig : public AudioInfo {
    const char* mime = nullptr;
    const char* ssid = nullptr;
    const char* password = nullptr;
    const char* path = "/";
    // optional input; if not used use write methods to push data
    Stream *input=nullptr;
    int port = 80;
};

/**
 * @brief A powerfull Web server which is based on 
 * https://github.com/pschatzmann/TinyHttp.
 * It supports multiple concurrent clients. You can e.g. use it to write mp3 data and make
 * it available in multiple clients.
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AudioServerEx : public AudioOutput {
  public:
    // Default Constructor
    AudioServerEx() = default;

    /// To be compatible with legacy API
    AudioServerEx(const char *ssid, const char* pwd){
        info.ssid = ssid;
        info.password = pwd;
    }

    virtual AudioServerExConfig defaultConfig() {
        AudioServerExConfig cfg;
        return cfg;
    }

    virtual bool begin(AudioServerExConfig cfg) {
        info = cfg;
        return begin();
    }

    virtual bool begin(Stream &in, const char* contentType) {
        info.input = &in;
        info.mime = contentType;
        return begin();
    }

    virtual bool begin() {
        end(); // we (re) start with  a clean state

        if (info.input==nullptr){
            p_stream = new ExtensionStream(info.path,tinyhttp::T_GET, info.mime );
        } else {
            p_stream = new ExtensionStream(info.path, info.mime, *info.input);
        }
        p_stream->setReplyHeader(*getReplyHeader()); 
        p_server = new tinyhttp::HttpServer(wifi);
        
        // handling of WAV
        p_server->addExtension(*p_stream);
        return p_server->begin(info.port, info.ssid, info.password);
    }

    virtual void end() {
        if (p_stream!=nullptr) {
            delete p_stream;
            p_stream = nullptr;
        }
        if (p_server!=nullptr) {
            delete p_server;
            p_server = nullptr;
        }
    }

    /// Web server supports write so that we can e.g. use is as destination for the audio player.
    size_t write(const uint8_t* data, size_t len) override {
        if (p_stream==nullptr) return 0;
        return p_stream->write((uint8_t*)data, len);
    }

    int availableForWrite() override {
        if (p_stream==nullptr) return 0;
        return p_stream->availableForWrite();
    }

    /// Needs to be called if the data was provided as input Stream in the AudioServerExConfig
    virtual void copy() {
        if (p_server!=nullptr){
            p_server->copy();
        }
    }

  protected:
    AudioServerExConfig info;
    WiFiServer wifi;
    HttpServer *p_server;
    ExtensionStream *p_stream=nullptr;

    virtual tinyhttp::Str* getReplyHeader() {
        return nullptr;
    }

};

/**
 * @brief A powerfull WAV Web server which is based on 
 * https://github.com/pschatzmann/TinyHttp.
 * It supports multiple concurrent clients
 * @ingroup http
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class AudioWAVServerEx : public AudioServerEx {
    public:
        // Default Constructor
        AudioWAVServerEx() = default;

        /// To be compatible with legacy API
        AudioWAVServerEx(const char *ssid, const char* pwd):AudioServerEx(ssid, pwd){}

        AudioServerExConfig defaultConfig() override {
            AudioServerExConfig cfg;
            cfg.mime = "audio/wav";
            return cfg;
        }

        /// Legacy API support
        bool begin(Stream &in, int sample_rate, int channels, int bits_per_sample=16) {
            info.input = &in;
            info.sample_rate = sample_rate;
            info.channels = channels;
            info. bits_per_sample = bits_per_sample;
            info.mime = "audio/wav";
            return AudioServerEx::begin();
        }

        bool begin(AudioServerExConfig cfg) override{
            return AudioServerEx::begin(cfg);
        }

    protected:
        // Dynamic memory
        tinyhttp::StrExt header;

        // wav files start with a 44 bytes header
        virtual tinyhttp::Str* getReplyHeader() {
            header.allocate(44);
            MemoryOutput mp{(uint8_t*)header.c_str(), 44};
            WAVHeader enc;
            WAVAudioInfo wi;
            wi.format = AudioFormat::PCM;
            wi.sample_rate = info.sample_rate;
            wi.bits_per_sample = info.bits_per_sample;
            wi.channels = info.channels;
            enc.setAudioInfo(wi);
            // fill header with data
            enc.writeHeader(&mp);
            // make sure that the length is 44
            assert(header.length() == 44);

            return &header;
        }
};

}