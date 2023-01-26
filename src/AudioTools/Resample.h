#pragma once

#include "AudioTools/AudioStreams.h"
#include <algorithm>    // std::max

namespace audio_tools {

/**
 * @brief Optional Configuration object. The critical information is the 
 * channels and the step_size. All other information is not used.
 * 
 */
struct ResampleConfig : public AudioBaseInfo {
    float step_size=1.0f;
    /// Optional fixed target sample rate
    int to_sample_rate = 0;
};

/**
 * @brief Dynamic Resampling. We can use a variable factor to speed up or slow down
 * the playback.
 * @author Phil Schatzmann
 * @ingroup transform
 * @copyright GPLv3
 * @tparam T 
 */
template<typename T>
class ResampleStream : public AudioStreamX {
  public:
    /// Support for resampling via write.
    ResampleStream(Print &out, int channelCount=2){
        setChannels(channelCount);
        p_out = &out;
    }
    /// Support for resampling via write. The audio information is copied from the io
    ResampleStream(AudioPrint &out){
        p_out = &out;
        setAudioInfo(out.audioInfo());
    }

    /// Support for resampling via write and read. 
    ResampleStream(Stream &io, int channelCount=2){
        setChannels(channelCount);
        p_out = &io;
        p_in = &io;
    }

    /// Support for resampling via write and read. The audio information is copied from the io
    ResampleStream(AudioStream &io){
        p_out = &io;
        p_in = &io;
        setAudioInfo(io.audioInfo());
    }

    // Provides the default configuraiton
    ResampleConfig defaultConfig() {
        ResampleConfig cfg;
        cfg.copyFrom(audioInfo());
        return cfg;
    }

    bool begin(ResampleConfig cfg){
        to_sample_rate = cfg.to_sample_rate;
        setAudioInfo(cfg);
        memset(last_samples.data(),0,sizeof(T)*info.channels);
        setStepSize(cfg.step_size);
        is_first = true;
        step_dirty = true;
        bytes_per_frame = info.bits_per_sample/8*info.channels;

        return true;
    }

    bool begin(AudioBaseInfo info, int fromRate, int toRate){
        ResampleConfig rcfg;
        rcfg.to_sample_rate = toRate;
        rcfg.step_size = getStepSize(fromRate, toRate);
        rcfg.copyFrom(info);
        return begin(rcfg);
    }

    bool begin(AudioBaseInfo info, float step){
        ResampleConfig rcfg;
        rcfg.step_size = step;
        rcfg.copyFrom(info);
        return begin(rcfg);
    }

    void setAudioInfo(AudioBaseInfo info) override {
        AudioStream::setAudioInfo(info);
        setChannels(info.channels);
        // update the step size if a fixed to_sample_rate has been defined
        if (to_sample_rate!=0){
            setStepSize(getStepSize(info.sample_rate, to_sample_rate));
        }
    }

    /// Defines the number of channels 
    void setChannels(int channels){
        last_samples.resize(channels);
        info.channels = channels;
    }

    /// influence the sample rate
    void setStepSize(float step){
        LOGI("setStepSize: %f", step);
        step_size = step;
    }

    /// calculate the step size the sample rate: e.g. from 44200 to 22100 gives a step size of 2 in order to provide fewer samples
    float getStepSize(float sampleRateFrom, float sampleRateTo){
        return sampleRateFrom / sampleRateTo;
    }

    /// Returns the actual step size
    float getStepSize(){
        return step_size;
    }

    int availableForWrite() override {
        return p_out->availableForWrite();
    }

    size_t write(const uint8_t* buffer, size_t bytes) override {
        size_t written;
        return write(p_out, buffer, bytes, written);
    }

    int available() override {
        return p_in!=nullptr ? p_in->available() : 0;
    }

    /// Reuses the write implementation to support readBytes
    size_t readBytes(uint8_t *buffer, size_t length) override {
        if (length==0) return 0;
        // setup ringbuffer size
        if (step_dirty){
            ring_buffer.resize(buffer_read_len * (bytes_per_frame+1) / bytes_per_frame);
            step_dirty = false;
        }
        // refill ringbuffer
        if (ring_buffer.available()<bytes_per_frame) {            
            size_t read_size = buffer_read_len * step_size * bytes_per_frame / bytes_per_frame;
            read_buffer.resize(read_size);
            // read data from source to buffer
            int bytes_read = p_in->readBytes(read_buffer.data(), read_size);
            if (bytes_read>0){
                size_t written=0;
                // resample to ringbuffer
                write(&ring_buffer, read_buffer.data(), read_size, written);
                LOGD("written: %d", (int)written);
            } else {
                LOGE("bytes_read==0");
            }
        }

        return ring_buffer.readBytes(buffer, length*bytes_per_frame / bytes_per_frame);
    }

    /// @brief  Defines the internal read buffer length that will be used to resample
    void setReadBufferLen(size_t len){
        buffer_read_len = len;
    }

    size_t getReadBufferLen(){
        return buffer_read_len;
    }


  protected:
    size_t buffer_read_len = 256;
    Print *p_out=nullptr;
    Stream *p_in=nullptr;
    Vector<T> last_samples{0};
    float idx=0;
    Vector<uint8_t> read_buffer{0};
    RingBufferStream ring_buffer{0};
    bool is_first = true;
    bool step_dirty = true;
    float step_size=1.0;
    int to_sample_rate = 0;
    int bytes_per_frame = 0;

    /// Writes the buffer to p_out after resampling
    size_t write(Print *p_out, const uint8_t* buffer, size_t bytes, size_t &written )  {
        // prevent npe
        if (info.channels==0){
            LOGE("channels is 0");
            return 0;
        }
        T* data = (T*)buffer;
        int samples = bytes / sizeof(T); 
        size_t frames = samples / info.channels;
        written = 0;

        // avoid noise if audio does not start with 0
        if (is_first){
            is_first = false;
            setupLastSamples(data, 0);
        }

        // process all samples
        while (idx<frames){
            for (int ch=0;ch<info.channels;ch++){
                T result = getValue(data, idx, ch);
                if (p_out->availableForWrite()<sizeof(T)){
                    LOGE("Could not write");
                }
                written += p_out->write((uint8_t*)&result, sizeof(T));
            }
            idx+=step_size;
        }
        
        // save last samples
        setupLastSamples(data, frames-1);
        idx -= frames;
        // returns requested bytes to avoid rewriting of processed bytes
        return bytes;
    }

    /// get the interpolated value for indicated (float) index value
    T getValue(T *data,float frame_idx, int channel){
        // interpolate value 
        int frame_idx1 = frame_idx;
        int frame_idx0 = frame_idx1-1;
        T val0 = lookup(data, frame_idx0, channel);
        T val1 = lookup(data, frame_idx1, channel);

        float result = mapFloat(frame_idx,frame_idx1,frame_idx0, val0, val1 );
        return round(result);
    }

    // lookup value for indicated frame & channel: index starts with -1;
    T lookup(T *data, int frame, int channel){
        if (frame>=0){
            return data[frame*info.channels+channel];
        } else {
            // index -1
            return last_samples[channel];
        }
    }  
    // store last samples to provide values for index -1
    void setupLastSamples(T *data, int frame){
        for (int ch=0;ch<info.channels;ch++){
            last_samples[ch] = data[(frame*info.channels)+ch];
        }
    }  
};

/**
 * @brief Dynamic Resampling. We can use a variable factor to speed up or slow down
 * the playback. This is the original implementation which should be slightly more efficient
 * for microcontrollers with slow floating point operations
 * @author Phil Schatzmann
 * @ingroup transform
 * @copyright GPLv3
 * @tparam T 
 */

template<typename T>
class ResampleStreamFast : public ResampleStream<T> {
  public:
    /// Support for resampling via write.
    ResampleStreamFast(Print &out, int channelCount=2) : ResampleStream<T>(out, channelCount){}
    /// Support for resampling via write. The audio information is copied from the io
    ResampleStreamFast(AudioPrint &out) : ResampleStream<T>(out) {}

    /// Support for resampling via write and read. 
    ResampleStreamFast(Stream &io, int channelCount=2) : ResampleStream<T>(io, channelCount){}

    /// Support for resampling via write and read. The audio information is copied from the io
    ResampleStreamFast(AudioStream &io) : ResampleStream<T>(io){}
    
    /// get the interpolated value for indicated (float) index value
    T getValue(T *data,float frame_idx, int channel)  {
        // provide value if number w/o digits
        if (frame_idx==(int)frame_idx)  {
            return ResampleStream<T>::lookup(data, frame_idx-1, channel);
        }
        // interpolate value 
        int frame_idx1 = frame_idx;
        int frame_idx0 = frame_idx1-1;
        T val0 = ResampleStream<T>::lookup(data, frame_idx0, channel);
        T val1 = ResampleStream<T>::lookup(data, frame_idx1, channel);
        T diff = val1 - val0;
        if (diff==0){
            // No difference
            return val0;
        } 
        // interpolate value
        float delta = (frame_idx - frame_idx0) - 1;
        T diffEffective = diff * delta;
        T result = val0+diffEffective;

        return result;
    }
};

} // namespace
