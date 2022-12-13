#pragma once

#include "AudioTools/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Optional Configuration object. The critical information is the 
 * channels and the step_size. All other information is not used.
 * 
 */
struct ResampleConfig : public AudioBaseInfo {
    float step_size=1;
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
        setAudioInfo(cfg);
        return begin(cfg.step_size);
    }

    bool begin(int fromRate, int toRate){
        return begin(getStepSize(fromRate, toRate));
    }

    bool begin(float step){
        memset(last_samples.data(),0,sizeof(T)*info.channels);
        setStepSize(step);
        is_first = true;
        return true;
    }

    void setAudioInfo(AudioBaseInfo info) override {
        AudioStream::setAudioInfo(info);
        setChannels(info.channels);
    }

    /// Defines the number of channels 
    void setChannels(int channels){
        last_samples.resize(channels);
        info.channels = channels;
    }

    /// influence the sample rate
    void setStepSize(float step){
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

    size_t readBytes(uint8_t *buffer, size_t length) override {
        // create buffer
        size_t write_len = length * step_size;
        read_buffer.resize(write_len);
        // read data from source to buffer
        int bytes_read = p_in->readBytes(read_buffer.data(), write_len);
        size_t written=0;
        if (bytes_read>0){
            // use use write implementation to fill buffer
            print_to_array.begin(buffer, length);
            write(&print_to_array, read_buffer.data(), bytes_read, written);
        }
        return written;
    }

  protected:
    Print *p_out=nullptr;
    Stream *p_in=nullptr;
    Vector<T> last_samples{0};
    float idx=0;
    Vector<uint8_t> read_buffer{0};
    AdapterPrintToArray print_to_array;
    bool is_first = true;
    float step_size=1;

    /// Writes the buffer to p_out after resampling
    size_t write(Print *p_out, const uint8_t* buffer, size_t bytes, size_t &written )  {
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
                written += p_out->write((uint8_t*)&result, sizeof(T));
            }
            idx+=step_size;
        }
        // save last samples
        setupLastSamples(data, frames-1);
        idx -= frames;
        return bytes;
    }

    // interpolate value
    inline T getValue(T *data,float frame_idx, int channel){
        int frame_idx1 = frame_idx;
        int frame_idx0 = frame_idx1-1;
        T val0 = lookup(data, frame_idx0, channel);
        T val1 = lookup(data, frame_idx1, channel);
        T diff = val1 - val0;
        T result;
        if (diff!=0){
            float delta = frame_idx - frame_idx0;
            T diffEffective = delta / diff;
            result = val0+diffEffective;
        } else {
            result = val0;
        }
        return result;
    }

    // lookup value for indicated frame & channel
    T lookup(T *data, int frame, int channel){
        if (frame>=0){
            return data[frame*info.channels+channel];
        } else {
            return last_samples[channel];
        }
    }  

    void setupLastSamples(T *data, int frame){
        for (int ch=0;ch<info.channels;ch++){
            last_samples[ch] = data[frame+ch];
        }
    }  
};



} // namespace
