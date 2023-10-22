#pragma once

#include "AudioConfig.h"
#include "AudioTools/AudioStreams.h"
#include "FEC/ReedSolomon/rs.hpp"

namespace audio_tools {

/**
 * @brief Forward error correction using Reed-Solomon: 
 * write is encoding and readBytes does the decoding.
 * @author Phil Schatzmann
 * @copyright GPLv3
 **/
template <int msglen, int ecclen>
class FECReedSolomon : public Stream {
  public:

    FECReedSolomon(Stream &stream){
        p_stream = &stream;
        p_print = &stream;
    }

    FECReedSolomon(Print &print){
        p_print = &print;
    }

    int availableForWrite() {
        return msglen;
    }
    
    size_t write(const uint8_t* data, size_t len){
        if (p_print==nullptr) return 0;
        for (int j=0;j<len;j++){
            raw.write(data[j]);
            if(raw.availableForWrite()==0){
                rs.Encode(raw.data(), encoded.data());
                p_print->write(encoded.data(), msglen+ecclen);
                raw.reset();
            }
        }
        return len;
    }

    int available() {
        return msglen;
    }

    size_t readBytes(uint8_t* data, size_t len){
        if (p_stream==nullptr) return 0;
        if (encoded.isEmpty()){
            int read_len = msglen + ecclen;
            p_stream->readBytes(encoded.data(), read_len);
            encoded.setAvailable(read_len);
        }
        return encoded.readArray(data, len);
    }

  protected:
    SingleBuffer<uint8_t> raw{msglen};
    SingleBuffer<uint8_t> encoded{msglen+ecclen};
    RS::ReedSolomon<msglen, ecclen> rs;
    Stream* p_stream = nullptr;
    Print* p_print = nullptr;

};

}