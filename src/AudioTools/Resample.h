#pragma once

#include "AudioTools/AudioStreams.h"

namespace audio_tools {

/**
 * @brief A simple implementation which changes the sample rate by the indicated integer factor. 
 * To downlample we calculate the avarage of n (=factor) samples. To upsample we interpolate
 * the missing samples. If the indicated factor is positive we upsample if it is negative
 * we downsample.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T data type of audio data
 */
template<typename T>
class Resample : public AudioStreamX {
    public:
        /**
         * @brief Construct a new Resample object
         * call setOut and begin to setup the required parameters
         */
        Resample() = default;
        /**
         * @brief Construct a new Converter Resample object
         * 
         * @param channels number of channels (default 2)
         * @param factor use a negative value to downsample
         */
        Resample(Print &out, int channels=2, int factor=2 ){
            setOut(out);
            begin(channels, factor);
        }
        /**
         * @brief Construct a new Resample object
         * 
         * @param in 
         * @param channels 
         * @param factor 
         */
        Resample(Stream &in, int channels=2, int factor=2 ){
            setIn(in);
            begin(channels, factor);
        }

        void begin(int channels=2, int factor=2) {
            this->channels = channels;
            this->factor = factor;
        }

        void setOut(Print &out){
            this->p_out = &out;
        }

        void setIn(Stream &in){
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
            size_t byte_count = 0;
            if (factor>1){
                int read_len = length/factor;
                int sample_count = read_len / sizeof(T);
                allocateBuffer(sample_count);
                read_len = p_in->readBytes((uint8_t*)buffer, read_len);
                sample_count = read_len / sizeof(T);
                byte_count = upsample(buffer,(T*)src, sample_count, channels, factor) * sizeof(T);
            } else if (factor<1){
                int abs_factor = abs(factor);
                int read_len = length * abs_factor;
                int sample_count = read_len / sizeof(T);
                allocateBuffer(sample_count);
                read_len = p_in->readBytes((uint8_t*)buffer, read_len);
                sample_count = read_len / sizeof(T);
                byte_count = downsample(buffer,(T*)src, sample_count, channels, abs_factor) * sizeof(T);
            } else {
                byte_count = p_in->readBytes(src, length);
            }
            return byte_count;
        }
   
    protected:
        Print *p_out=nullptr;
        Stream *p_in=nullptr;
        T *buffer=nullptr;
        T *last_end=nullptr;
        int channels = 2;
        int factor = 1;
        int buffer_size = 0;

        // allocates a buffer; len is specified in samples
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
            int to_pos=0;
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
                    to_pos = j/factor;
                    *p_data(to_pos, ch, to) = total[ch] / factor;
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
                        T tmp = actual_data + (diff*f);
                        *p_data(pos, ch, to) = tmp; 
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

enum ResamplePrecision { Low, Medium, High, VeryHigh};

/**
 * @brief Class to determine a combination of upsample and downsample rates to achieve any ratio
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ResampleParameterEstimator {
    public:

        ResampleParameterEstimator() = default;

        ResampleParameterEstimator(int fromRate, int toRate, ResamplePrecision precision = Medium){
            begin(fromRate, toRate, precision);
        }

        void begin(int fromRate, int toRate, ResamplePrecision precision = Medium){
            this->from_rate = fromRate;
            this->to_rate = toRate;
            this->precision = precision;
            // update result values
            calculate();
        }

        /// prposed factor for upsampling
        int factor() {
            return fact;
        }

        /// propose divisor for downsampling
        int divisor() {
            return div;
        }

        /// original sample rate
        int fromRate(){
            return from_rate;
        }

        /// target sample rate
        int toRate(){
            return to_rate;
        }

        /// effective target sample rate by upsampling and then downsampling at different factors
        float toRateEffective() {
            return to_rate_eff;
        }

        /// same as factor
        int upsample() {
            return factor();
        }

        /// same as division but provides negative number to indicate that we need to downsample
        int downsample() {
            return - divisor();
        }

        /// Determines a supported downsampling write size
        size_t supportedSize(size_t len){
            return (len / div) * div;
        }

    protected:
        int fact=0, div=0;
        int from_rate=0, to_rate=0;
        float diff=10000000.0;
        float to_rate_eff=0;
        int div_array[28] = {1, 2, 3, 5, 7, 10, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 100};
        int limits[4] = {6, 12, 18, 27 }; 
        int precision=1;

        // find the parameters with the lowest difference
        void calculate() {
            int limit_max = limits[precision];
            for (int j=0;j<limit_max;j++){
                int tmp_div = div_array[j];
                int tmp_fact = rintf(static_cast<float>(to_rate) * tmp_div / from_rate);
                float tmp_diff = static_cast<float>(to_rate) - (static_cast<float>(from_rate) * tmp_fact / tmp_div);
                LOGD("div: %d, fact %d -> diff: %f", tmp_div,tmp_fact,tmp_diff);
                if (abs(tmp_diff)<abs(diff)){
                    fact = tmp_fact;
                    div = tmp_div;
                    diff = tmp_diff;
                    if (diff==0.0){
                        break; 
                    }
                }
            }
            to_rate_eff = static_cast<float>(from_rate)  * fact / div;
            LOGI("div: %d, fact %d -> rate: %d, rate_eff: %f", div,fact,to_rate, to_rate_eff);
        }
};
/**
 * @brief Configuration for ResampleStream
 * 
 */
struct ResampleConfig : public AudioBaseInfo {
    int sample_rate_from=0;
};

/**
 * @brief Flexible Stream class which can be used to resample audio data between
 * different sample rates.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T data type of audio data
 */
template<typename T>
class ResampleStream : public AudioStreamX {
    public:
        /**
         * @brief Construct a new Resample Stream object which supports resampling
         * on it's write operations
         * 
         * @param out 
         * @param precision 
         */
        ResampleStream(Print &out, ResamplePrecision precision = Medium){
            this->precision = precision;
            up.setOut(down); // we upsample first
            down.setOut(out); // so that we can downsample to the requested rate
        }

        /**
         * @brief Construct a new Resample Stream object which supports resampling
         * both on read and write.
         * 
         * @param out 
         * @param precision 
         */
        ResampleStream(Stream &in, ResamplePrecision precision = Medium){
            this->precision = precision;
            up.seIn(down); // we upsample first
            down.setIn(in); // so that we can downsample to the requested rate
        }

        /// Returns an empty configuration object
        ResampleConfig defaultConfig() {
            ResampleConfig cfg;
            return cfg;
        }

        // Recalculates the up and downsamplers
        bool begin() {
            if (channels==0){
                LOGE("channels are not defined")
                return false;
            }
            if (from_rate==0){
                LOGE("from_rate is not defined")
                return false;
            }
            if (to_rate==0){
                LOGE("to_rate is not defined")
                return false;
            }
            calc.begin(from_rate, to_rate, precision);
            up.begin(channels, calc.upsample());
            down.begin(channels, calc.downsample());
            return true;
        }

        /// Defines the channels and sample rates
        bool begin(int channels, int fromRate, int toRate){
            this->channels = channels;
            this->from_rate = fromRate;
            this->to_rate = toRate;
            return begin();
        }

        /// Defines the channels and sample rates from the AudioBaseInfo. Please call setFromInfo() or setFromSampleRate() before
        bool begin(ResampleConfig info){
            channels = info.channels;
            from_rate = info.sample_rate_from;
            to_rate = info.sample_rate;
            return begin();
        }

        /// Handle Change of the source sampling rate - and channels!
        void setAudioInfo(AudioBaseInfo info) override {
            from_rate = info.sample_rate;
            channels = info.channels;
            begin();
        }

        /// Determines the number of bytes which are available for write 
        int availableForWrite() override { return up.availableForWrite(); }

        /// Writes the data up or downsampled to the final destination
        size_t write(const uint8_t *src, size_t byte_count) override {
            return up.write(src, byte_count);
        }
        /// Determines the available bytes from the final source stream 
        int available() override { return up.available(); }

        /// Reads the up/downsampled bytes
        size_t readBytes(uint8_t *src, size_t byte_count) override { 
            return up.readBytes(src, byte_count);
        }

    protected:
        int channels, from_rate, to_rate;
        ResampleParameterEstimator calc;
        Resample<T> up;
        Resample<T> down;
        ResamplePrecision precision;

};

} // namespace
