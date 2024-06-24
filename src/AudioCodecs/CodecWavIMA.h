#pragma once

#include "AudioCodecs/AudioCodecsBase.h"

#define WAVE_FORMAT_IMA_ADPCM 0x0011
#define TAG(a, b, c, d) ((static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(c) << 8) | (d))
#define READ_BUFFER_SIZE 512

namespace audio_tools {

const int16_t ima_index_table[16] {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

const int32_t ima_step_table[89] {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

/**
 * @brief Sound information which is available in the WAV header - adjusted for IMA ADPCM
 * @author Phil Schatzmann
 * @author Norman Ritz
 * @copyright GPLv3
 * 
 */
struct WavIMAAudioInfo : AudioInfo {
    WavIMAAudioInfo() = default;
    WavIMAAudioInfo(const AudioInfo& from) {
        sample_rate = from.sample_rate;    
        channels = from.channels;       
        bits_per_sample = from.bits_per_sample; 
    }

    int format = WAVE_FORMAT_IMA_ADPCM;
    int byte_rate = 0;
    int block_align = 0;
    int frames_per_block = 0;
    int num_samples = 0;
    bool is_valid = false;
    uint32_t data_length = 0;
    uint32_t file_size = 0;
};

struct IMAState {
    int32_t predictor = 0;
    int step_index = 0;
};

const char* wav_ima_mime = "audio/x-wav";

/**
 * @brief Parser for Wav header data adjusted for IMA ADPCM format - partially based on CodecWAV.h
 * for details see https://de.wikipedia.org/wiki/RIFF_WAVE
 * @author Phil Schatzmann
 * @author Norman Ritz
 * @copyright GPLv3
 */

typedef enum {
    IMA_ERR_INVALID_CHUNK = -2,
    IMA_ERR_INVALID_CONTAINER,
    IMA_CHUNK_OK,
    IMA_CHUNK_UNKNOWN
} chunk_result;

class WavIMAHeader  {
    public:
        WavIMAHeader() {
            clearHeader();
        };

        void clearHeader() {
            data_pos = 0;
            memset((void*)&headerInfo, 0, sizeof(WavIMAAudioInfo));
            headerInfo.is_valid = false;
            header_complete = false;
            chunk_len = 0;
            max_chunk_len = 8;
            skip_len = 0;
            isFirstChunk = true;
        }

        chunk_result parseChunk() {
            data_pos = 0;
            bool chunkUnknown = false;
            uint32_t tag = read_tag();
            uint32_t length = read_int32();
            if (length < 4) {
                return IMA_ERR_INVALID_CHUNK;
            }
            if (tag == TAG('R', 'I', 'F', 'F')) {
                uint32_t container_type = read_tag();
                if (container_type != TAG('W', 'A', 'V', 'E')) {
                    return IMA_ERR_INVALID_CONTAINER;
                }
            }
            else if (tag == TAG('f', 'm', 't', ' ')) {
                if (length < 20) {
                    // Insufficient data for 'fmt '
                    return IMA_ERR_INVALID_CHUNK;
                }
                headerInfo.format          = read_int16();
                headerInfo.channels        = read_int16();
                headerInfo.sample_rate     = read_int32();
                headerInfo.byte_rate       = read_int32();
                headerInfo.block_align     = read_int16();
                headerInfo.bits_per_sample = read_int16();

                // Skip the size parameter for extra information as for IMA ADPCM the following data should always be 2 bytes.
                skip(2);
                headerInfo.frames_per_block = read_int16();
                if (headerInfo.format != WAVE_FORMAT_IMA_ADPCM || headerInfo.channels > 2) {
                    // Insufficient or invalid data for waveformatex
                    LOGE("Format not supported: %d, %d\n", headerInfo.format, headerInfo.channels);
                    return IMA_ERR_INVALID_CHUNK;
                } else {
                    headerInfo.is_valid = true; // At this point we know that the format information is valid
                }
            } else if (tag == TAG('f', 'a', 'c', 't')) {
                /* In the context of ADPCM the fact chunk should contain the total number of mono or stereo samples
                    however we shouldn't rely on this as some programs (e.g. Audacity) write an incorrect value in some cases. This value is currently not used by the decoder.
                */
                headerInfo.num_samples = read_int32();
            } else if (tag == TAG('d', 'a', 't', 'a')) {
                // Size of the data chunk.
                headerInfo.data_length = length;
            } else {
                chunkUnknown = true;
            }
            // Skip any remaining data that exceeds the buffer
            if (tag != TAG('R', 'I', 'F', 'F') && length > 20) skip_len = length - 20;
            return chunkUnknown ? IMA_CHUNK_UNKNOWN : IMA_CHUNK_OK;
        }

        /* Adds data to the header data buffer
           Because the header isn't necessarily uniform, we go through each chunk individually
           and only copy the ones we need. This could probably still be optimized. */
        int write(uint8_t* data, size_t data_len) {
            int write_len;
            int data_offset = 0;
            while (data_len > 0 && !header_complete) {
                if (skip_len > 0) {
                    /* Used to skip any unknown chunks or chunks that are longer than expected.
                       Some encoders like ffmpeg write meta information before the "data" chunk by default. */
                    write_len = min(skip_len, data_len);
                    skip_len -= write_len;
                    data_offset += write_len;
                    data_len -= write_len;
                }
                else {
                    // Search / Wait for the individual chunks and write them to the temporary buffer.
                    write_len = min(data_len, max_chunk_len - chunk_len);
                    memmove(chunk_buffer + chunk_len, data + data_offset, write_len);
                    chunk_len += write_len;
                    data_offset += write_len;
                    data_len -= write_len;

                    if (chunk_len == max_chunk_len) {
                        data_pos = 0;
                        if (max_chunk_len == 8) {
                            uint32_t chunk_tag = read_tag();
                            uint32_t chunk_size = read_int32();
                            if (isFirstChunk && chunk_tag != TAG('R', 'I', 'F', 'F')) {
                                headerInfo.is_valid = false;
                                return IMA_ERR_INVALID_CONTAINER;
                            }
                            isFirstChunk = false;
                            if (chunk_tag == TAG('R', 'I', 'F', 'F')) chunk_size = 4;
                            else if (chunk_tag == TAG('d', 'a', 't', 'a')) {
                                parseChunk();
                                header_complete = true;
                                logInfo();
                                break;
                            }

                            /* Wait for the rest of the data before processing the chunk.
                               The largest chunk we expect is the "fmt " chunk which is 20 bytes long in this case. */
                            write_len = min((size_t)chunk_size, (size_t)20);
                            max_chunk_len += write_len;
                            continue;
                        }
                        else {
                            chunk_result result = parseChunk();
                            switch (result) {
                                // Abort processing the header if the RIFF container or a required chunk is not valid
                                case IMA_ERR_INVALID_CONTAINER:
                                case IMA_ERR_INVALID_CHUNK:
                                headerInfo.is_valid = false;
                                return result;
                                break;
                            }
                            chunk_len = 0;
                            max_chunk_len = 8;
                        }
                    }
                }
            }
            return data_offset;
        }

        /// Returns true if the header is complete (data chunk has been found)
        bool isDataComplete() {
            return header_complete;
        }

        // provides the AudioInfo
        WavIMAAudioInfo &audioInfo() {
            return headerInfo;
        }

    protected:
        struct WavIMAAudioInfo headerInfo;
        uint8_t chunk_buffer[28];
        size_t chunk_len = 0;
        size_t max_chunk_len = 8;
        size_t skip_len = 0;
        size_t data_pos = 0;
        bool header_complete = false;
        bool isFirstChunk = true;

        uint32_t read_tag() {
            uint32_t tag = getChar();
            tag = (tag << 8) | getChar();
            tag = (tag << 8) | getChar();
            tag = (tag << 8) | getChar();
            return tag;
        }

        uint32_t read_int32() {
            uint32_t value = (uint32_t)getChar();
            value |= (uint32_t)getChar() << 8;
            value |= (uint32_t)getChar() << 16;
            value |= (uint32_t)getChar() << 24;
            return value;
        }

        uint16_t read_int16() {
            uint16_t value = getChar();
            value |= getChar() << 8;
            return value;
        }

        void skip(int n) {
            n = min((size_t)n, chunk_len - data_pos);
            for (int i=0; i<n; i++) if (data_pos < chunk_len) data_pos++;
            return;
        }

        int getChar() {
            if (data_pos < chunk_len) return chunk_buffer[data_pos++];
            else return -1;
        }

        void logInfo() {
            LOGI("WavIMAHeader format: %d", headerInfo.format);
            LOGI("WavIMAHeader channels: %d", headerInfo.channels);
            LOGI("WavIMAHeader sample_rate: %d", headerInfo.sample_rate);
            LOGI("WavIMAHeader block align: %d", headerInfo.block_align);
            LOGI("WavIMAHeader bits_per_sample: %d", headerInfo.bits_per_sample);
        }
};


/**
 * @brief Obsolete: WavIMADecoder - based on WAVDecoder - We parse the header data as we receive it
 * and send the sound data to the stream which was indicated in the constructor.
 * Only WAV files with WAVE_FORMAT_IMA_ADPCM are supported by this codec!
 * 
 * We recommend using the WAVDecoder with a corresponding ADPCMDecoder instead. 
 * 
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @author Norman Ritz
 * @copyright GPLv3
 */
class WavIMADecoder : public AudioDecoder {
    public:
        /**
         * @brief Construct a new WavIMADecoder object
         */

        WavIMADecoder() {
            TRACED();
        }

        /**
         * @brief Construct a new WavIMADecoder object
         * 
         * @param out_stream Output Stream to which we write the decoded result
         */
        WavIMADecoder(Print &out_stream, bool active=true) {
            TRACED();
            this->out = &out_stream;
            this->active = active;
        }

        /**
         * @brief Construct a new WavIMADecoder object
         * 
         * @param out_stream Output Stream to which we write the decoded result
         * @param bi Object that will be notified about the Audio Formt (Changes)
         */

        WavIMADecoder(Print &out_stream, AudioInfoSupport &bi) {
            TRACED();
            this->out = &out_stream;
            addNotifyAudioChange(bi);
        }

        ~WavIMADecoder() {
            if (input_buffer != nullptr) delete[] input_buffer;
            if (output_buffer != nullptr) delete[] output_buffer;
        }

        /// Defines the output Stream
        void setOutput(Print &out_stream) {
            this->out = &out_stream;
        }

        bool begin() {
            TRACED();
            ima_states[0].predictor = 0;
            ima_states[0].step_index = 0;
            ima_states[1].predictor = 0;
            ima_states[1].step_index = 0;
            isFirst = true;
            active = true;
            header.clearHeader();
            return true;
        }

        void end() {
            TRACED();
            active = false;
        }

        const char* mime() {
            return wav_ima_mime;
        }

        WavIMAAudioInfo &audioInfoEx() {
            return header.audioInfo();
        }

        AudioInfo audioInfo() override {
            return header.audioInfo();
        }

        virtual size_t write(const uint8_t *data, size_t len) {
            TRACED();
            if (active) {
                if (isFirst) {
                    // we expect at least the full header
                    int written = header.write((uint8_t*)data, len);
                    if (written == IMA_ERR_INVALID_CONTAINER || written == IMA_ERR_INVALID_CHUNK) {
                        isValid = false;
                        isFirst = false;
                        LOGE("File is not valid");
                        return len;
                    }

                    if (!header.isDataComplete()) {
                        return len;
                    }

                    size_t len_open = len - written;
                    uint8_t *sound_ptr = (uint8_t *) data + written;
                    isFirst = false;
                    isValid = header.audioInfo().is_valid;

                    LOGI("WAV sample_rate: %d", header.audioInfo().sample_rate);
                    LOGI("WAV data_length: %u", (unsigned) header.audioInfo().data_length);
                    LOGI("WAV is_valid: %s", header.audioInfo().is_valid ? "true" :  "false");

                    isValid = header.audioInfo().is_valid;
                    if (isValid) {
                        if (input_buffer != nullptr) delete[] input_buffer;
                        if (output_buffer != nullptr) delete[] output_buffer;
                        bytes_per_encoded_block = header.audioInfo().block_align;
                        bytes_per_decoded_block = header.audioInfo().frames_per_block * header.audioInfo().channels * 2;
                        samples_per_decoded_block = bytes_per_decoded_block >> 1;
                        input_buffer = new uint8_t[bytes_per_encoded_block];
                        output_buffer = new int16_t[samples_per_decoded_block];
                        // update sampling rate if the target supports it
                        AudioInfo bi;
                        bi.sample_rate = header.audioInfo().sample_rate;
                        bi.channels = header.audioInfo().channels;
                        bi.bits_per_sample = 16;
                        remaining_bytes = header.audioInfo().data_length;
                        notifyAudioChange(bi);
                        // write prm data from first record
                        LOGI("WavIMADecoder writing first sound data");
                        processInput(sound_ptr, len_open);
                    }
                } else if (isValid) {
                    processInput((uint8_t*)data, len);
                }
            }
            return len;
        }

        /// Alternative API which provides the data from an input stream
        int readStream(Stream &in) {
            TRACED();
            uint8_t buffer[READ_BUFFER_SIZE];
            int len = in.readBytes(buffer, READ_BUFFER_SIZE);
            return write(buffer, len);
        }

        virtual operator bool() {
            return active;
        }

    protected:
        WavIMAHeader header;
        Print *out;
        bool isFirst = true;
        bool isValid = true;
        bool active;
        uint8_t *input_buffer = nullptr;
        int32_t input_pos = 0;
        size_t remaining_bytes = 0;
        size_t bytes_per_encoded_block = 0;
        int16_t *output_buffer = nullptr;
        size_t bytes_per_decoded_block = 0;
        size_t samples_per_decoded_block = 0;
        IMAState ima_states[2];

        int16_t decodeSample(uint8_t sample, int channel = 0) {
            int step_index = ima_states[channel].step_index;
            int32_t step = ima_step_table[step_index];
            step_index += ima_index_table[sample];
            if (step_index < 0) step_index = 0;
            else if (step_index > 88) step_index = 88;
            ima_states[channel].step_index = step_index;
            int32_t predictor = ima_states[channel].predictor;
            uint8_t sign = sample & 8;
            uint8_t delta = sample & 7;
            int32_t diff = step >> 3;
            if (delta & 4) diff += step;
            if (delta & 2) diff += (step >> 1);
            if (delta & 1) diff += (step >> 2);
            if (sign) predictor -= diff;
            else predictor += diff;
            if (predictor < -32768) predictor = -32768;
            else if (predictor > 32767) predictor = 32767;
            ima_states[channel].predictor = predictor;
            return (int16_t)predictor;
        }

        void decodeBlock(int channels) {
            if (channels == 0 || channels > 2) return;
            input_pos = 4;
            int output_pos = 1;
            ima_states[0].predictor = (int16_t)((input_buffer[1] << 8) + input_buffer[0]);
            ima_states[0].step_index = input_buffer[2];
            output_buffer[0] = ima_states[0].predictor;
            if (channels == 2) {
                ima_states[1].predictor = (int16_t)(input_buffer[5] << 8) + input_buffer[4];
                ima_states[1].step_index = input_buffer[6];
                output_buffer[1] = ima_states[1].predictor;
                input_pos = 8;
                output_pos = 2;
            }
            for (int i=0; i<samples_per_decoded_block-channels; i++) {
                uint8_t sample = (i & 1) ? input_buffer[input_pos++] >> 4 : input_buffer[input_pos] & 15;
                if (channels == 1) output_buffer[output_pos++] = decodeSample(sample);
                else {
                    output_buffer[output_pos] = decodeSample(sample, (i >> 3) & 1);
                    output_pos += 2;
                    if ((i & 15) == 7) output_pos -= 15;
                    else if ((i & 15) == 15) output_pos--;
                }
            }
        }

        void processInput(const uint8_t* data, size_t size) {
            int max_size = min(size, remaining_bytes);
            for (int i=0; i<max_size; i++) {
                input_buffer[input_pos++] = data[i];
                if (input_pos == bytes_per_encoded_block) {
                    decodeBlock(header.audioInfo().channels);
                    input_pos = 0;
                    out->write((uint8_t*)output_buffer, bytes_per_decoded_block);
                }
            }
            remaining_bytes -= max_size;
            if (remaining_bytes == 0) active = false;
        }
};

}
