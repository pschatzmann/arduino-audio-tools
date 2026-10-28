#pragma once

#include "AudioToolsConfig.h"
#include "AudioTools/CoreAudio/BaseStream.h"
#include "FEC/ReedSolomon/rs.hpp"

namespace audio_tools {

/**
 * @brief Forward error correction using Reed-Solomon: write is encoding and
 * readBytes does the decoding.
 *
 * The written data is split into blocks of bytecount bytes and each block is
 * written with additional_bytes parity bytes. Data is only sent in complete
 * blocks: the bytes of a trailing incomplete block (less than bytecount
 * bytes) are never written. The reader collects the encoded blocks from
 * the stream and corrects up to additional_bytes / 2 wrong bytes per block.
 * If a block can not be corrected, the uncorrected data bytes are provided
 * and failedBlocks() is incremented. There are no sync markers: if bytes get
 * lost on the link, the block boundaries are wrong until begin() is called
 * on both sides to restart at a block boundary.
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

    /// Resets the encoding and decoding state: can be used to resynchronize
    /// the block boundaries (e.g. after lost bytes)
    bool begin() override {
        raw_len = 0;
        rx_len = 0;
        decoded_pos = 0;
        decoded_len = 0;
        failed_blocks = 0;
        return true;
    }

    /// Number of bytes which can be written until the next block is encoded
    int availableForWrite() override {
        return bytecount - raw_len;
    }

    size_t write(const uint8_t* data, size_t len) override {
        if (p_print==nullptr) return 0;
        for (size_t j=0;j<len;j++){
            raw[raw_len++] = data[j];
            if(raw_len == bytecount){
                rs.Encode(raw, tx_encoded);
                raw_len = 0;
                if (!writeBlock()) return j + 1;
            }
        }
        return len;
    }

    /// Number of decoded bytes which can be read without blocking
    int available() override {
        if (p_stream==nullptr) return 0;
        if (decoded_pos == decoded_len) {
            // read the available encoded data without blocking
            int open = bytecount + additional_bytes - rx_len;
            int avail = p_stream->available();
            if (avail > 0) {
                int n = avail < open ? avail : open;
                rx_len += p_stream->readBytes(rx_encoded + rx_len, n);
                decodeBlock();
            }
        }
        return decoded_len - decoded_pos;
    }

    size_t readBytes(uint8_t* data, size_t len) override{
        if (p_stream==nullptr) return 0;
        size_t result = 0;
        while (result < len) {
            if (decoded_pos == decoded_len) {
                // collect a complete encoded block
                int open = bytecount + additional_bytes - rx_len;
                int n = p_stream->readBytes(rx_encoded + rx_len, open);
                if (n <= 0) break;
                rx_len += n;
                if (!decodeBlock()) continue;
            }
            int n = decoded_len - decoded_pos;
            if ((size_t)n > len - result) n = len - result;
            memcpy(data + result, decoded + decoded_pos, n);
            decoded_pos += n;
            result += n;
        }
        return result;
    }

    /// Number of blocks which could not be corrected
    size_t failedBlocks() { return failed_blocks; }

  protected:
    // write side
    uint8_t raw[bytecount];
    int raw_len = 0;
    uint8_t tx_encoded[bytecount + additional_bytes];
    // read side
    uint8_t rx_encoded[bytecount + additional_bytes];
    int rx_len = 0;
    uint8_t decoded[bytecount];
    int decoded_pos = 0;
    int decoded_len = 0;
    size_t failed_blocks = 0;
    const int max_write_retries = 1000;
    RS::ReedSolomon<bytecount, additional_bytes> rs;
    Stream* p_stream = nullptr;
    Print* p_print = nullptr;

    /// Writes the complete encoded block: a partial block would break the
    /// block boundaries of the receiver
    bool writeBlock() {
        int total = bytecount + additional_bytes;
        int written = 0;
        int retry = 0;
        while (written < total) {
            int n = p_print->write(tx_encoded + written, total - written);
            if (n > 0) {
                written += n;
                retry = 0;
            } else if (++retry > max_write_retries) {
                LOGE("ReedSolomonFEC: could not write block: %d of %d bytes", written, total);
                return false;
            } else {
                delay(1);
            }
        }
        return true;
    }

    /// Decodes the received block if it is complete: returns false if more
    /// data is needed
    bool decodeBlock() {
        if (rx_len < bytecount + additional_bytes) return false;
        if (rs.Decode(rx_encoded, decoded) != 0) {
            // provide the uncorrected data bytes
            LOGW("ReedSolomonFEC: block could not be corrected");
            memcpy(decoded, rx_encoded, bytecount);
            failed_blocks++;
        }
        rx_len = 0;
        decoded_pos = 0;
        decoded_len = bytecount;
        return true;
    }

};

}
