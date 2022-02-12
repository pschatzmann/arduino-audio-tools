#pragma once

#include "AudioTools/AudioStreams.h"
/**
 * @brief A simple implementation which changes the sample rate by the indicated factor. 
 * To downlample we calculate the avarage of n (=factor) samples. To upsample we interpolate
 * the missing samples. If the indicated factor is positive we upsample if it is negative
 * we downsample.
 * 
 * @tparam T 
 */
template<typename T>
class Resample : public AudioStreamX {
    public:
        /**
         * @brief Construct a new Converter Resample object
         * 
         * @param channels number of channels (default 2)
         * @param factor use a negative value to downsample
         */
        Resample(Print &out, int channels=2, int factor=2 ){
            this->channels = channels;
            this->factor = factor;
            this->p_out = &out;
        }

        Resample(Stream &in, int channels=2, int factor=2 ){
            this->channels = channels;
            this->factor = factor;
            this->p_out = &in;
            this->p_in = &in;
        }

        int availableForWrite() override { return p_out->availableForWrite(); }

        /// Writes the data up or downsampled to the final destination
        size_t write(const uint8_t *src, size_t byte_count) override {
            if (byte_count%channels!=0){
                LOGE("Invalid buffer size: It must be multiple of %d", channels);
                return 0;
            }
            size_t bytes = 0;
            size_t result = 0;
            int sample_count = byte_count / sizeof(T);
            if (factor>1){
                allocateBuffer(sample_count*factor);
                bytes = upsample((T*)src, buffer, sample_count, channels, factor) * sizeof(T);
                result = p_out->write((uint8_t*)buffer, bytes) / factor;
            } else if (factor<1){
                int abs_factor = abs(factor);
                allocateBuffer(sample_count/abs_factor);
                bytes = downsample((T*)src, buffer , sample_count, channels, abs_factor) * sizeof(T);
                result = p_out->write((uint8_t*)buffer, bytes) * abs_factor;
            } else {
                result = p_out->write(src, byte_count);
            }
            return result;
        }

        /// Determines the available bytes from the final source stream 
        int available() override { return p_in!=nullptr ? p_in->available() : 0; }

        /// Reads the up/downsampled bytes
        size_t readBytes(uint8_t *src, size_t length) override { 
            if (p_in==nullptr) return 0;
            if (length%channels!=0){
                length = length / channels * channels;
            }
            size_t bytes = 0;
            int size = length / channels / sizeof(T);
            if (factor>1){
                int read_len = length/factor;
                allocateBuffer(read_len);
                read_len = p_in->readBytes((uint8_t*)buffer, read_len);
                bytes = upsample(buffer,(T*)src, read_len, channels, factor) * sizeof(T);
            } else if (factor<1){
                int abs_factor = abs(factor);
                int read_len = length*factor;
                allocateBuffer(read_len);
                read_len = p_in->readBytes((uint8_t*)buffer, read_len);
                bytes = downsample(buffer,(T*)src, read_len, channels, abs_factor) * sizeof(T);
            } else {
                bytes = p_in->readBytes(src, length);
            }
            return bytes;
        }
   

    protected:
        Print *p_out=nullptr;
        Stream *p_in=nullptr;
        T *buffer=nullptr;
        T *last_end=nullptr;
        int channels = 2;
        int factor = 1;
        int buffer_size = 0;

        void allocateBuffer(int len) {
            if (len>buffer_size){
                if (buffer!=nullptr) delete []buffer;
                buffer = new T[len];
                buffer_size = len;
            }
            if (last_end==nullptr){
                last_end = new T[channels];
            }
        }

        // reduces the sampes by the indicated factor. 
        size_t downsample(T *from,T *to, int sample_count, int channels, int factor ){
            if (sample_count%factor!=0){
                LOGE("Incompatible buffer length for down sampling. If must be a factor of %d", factor);
                return 0;
            }
            int frame_count = sample_count / channels;
            size_t result = 0;
            for (int16_t j=0; j<frame_count; j+=factor){
                long total[channels];
                for (int8_t ch=0; ch<channels; ch++){
                    total[ch]=0;
                    int pos = j+ch;
                    for (int16_t f=0; f<factor; f++){
                        total[ch] += *p_data(j+f, ch, from); 
                    }
                    *p_data(j/factor, ch, to) = total[ch] / factor;
                    result++;
                }
            }
            return result;
        }

        /// Increases the samples by the indicated factor: We interpolate the missing samples
        size_t upsample(T *from, T* to, int sample_count, int channels, int factor ){
            int frame_count = sample_count/channels;
            size_t result = 0;
            int pos = 0;
            // we start with the last saved frame
            for (int16_t frame_pos=-1; frame_pos<frame_count-1; frame_pos++){
                for (int8_t ch=0; ch<channels; ch++){
                    T actual_data = *p_data(frame_pos, ch, from);
                    T next_data = *p_data(frame_pos+1, ch, from);
                    float diff = (next_data - actual_data) / factor;
                    pos =(frame_pos+1)*factor; 
                    *p_data(pos, ch, to) = actual_data; 
                    result++;
                    for (int16_t f=1;f<factor;f++){
                        pos = ((frame_pos+1)*factor)+f; 
                        *p_data(pos, ch, to) = actual_data + (diff*f); 
                        result++;
                    }
                }
            }
            // save last frame_pos data
            pos = frame_count*factor;
            for (int8_t ch=0; ch<channels; ch++){
                T tmp = *p_data(frame_count-1, ch, from); 
                last_end[ch] = tmp;
            }
            return pos*channels;
        }

        // provide access to data
        T* p_data(int frame_pos, int channel, T*start){
            return frame_pos>=0 ? start+(frame_pos*channels)+channel : last_end+channel;
        }

};