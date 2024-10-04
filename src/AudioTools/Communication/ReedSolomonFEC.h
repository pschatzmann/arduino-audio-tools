#pragma once

#include "AudioConfig.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "FEC/ReedSolomon/rs.hpp"

namespace audio_tools {

/**
 * @brief Forward error correction using Reed-Solomon: 
 * write is encoding and readBytes does the decoding.
 * @ingroup fec
 * @author Phil Schatzmann
 * @copyright GPLv3
 **/
template <int bytecount, int additional_bytes>
class ReedSolomonFEC : public BaseStream {
  public:

    ReedSolomonFEC(Stream &stream){
        p_stream = &stream;
        p_print = &stream;
    }

    ReedSolomonFEC(Print &print){
        p_print = &print;
    }

    int availableForWrite() override {
        return bytecount;
    }
    
    size_t write(const uint8_t* data, size_t len) override {
        if (p_print==nullptr) return 0;
        for (int j=0;j<len;j++){
            raw.write(data[j]);
            if(raw.availableForWrite()==0){
                rs.Encode(raw.data(), encoded.data());
                p_print->write(encoded.data(), bytecount+additional_bytes);
                raw.reset();
            }
        }
        return len;
    }

    int available()override {
        return bytecount;
    }

    size_t readBytes(uint8_t* data, size_t len) override{
        if (p_stream==nullptr) return 0;
        if (encoded.isEmpty()){
            int read_len = bytecount + additional_bytes;
            p_stream->readBytes(encoded.data(), read_len);
            encoded.setAvailable(read_len);
        }
        return encoded.readArray(data, len);
    }


  protected:
    SingleBuffer<uint8_t> raw{bytecount};
    SingleBuffer<uint8_t> encoded{bytecount+additional_bytes};
    RS::ReedSolomon<bytecount, additional_bytes> rs;
    Stream* p_stream = nullptr;
    Print* p_print = nullptr;

};

}