#pragma once
#include "AudioConfig.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Converter.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/AudioStreams.h"
#include "AudioBasic/Int24.h"

#define MAX_SINGLE_CHARS 8

namespace audio_tools {

#if !defined(ARDUINO) || defined(IS_DESKTOP)
#  define FLUSH_OVERRIDE override
#else
#  define FLUSH_OVERRIDE 
#endif


/**
 * @brief Abstract Audio Ouptut class
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioPrint : public Print, public AudioBaseInfoDependent, public AudioBaseInfoSource {
    public:
        virtual ~AudioPrint() = default;
         size_t write(const uint8_t *buffer, size_t size) override = 0;

        virtual size_t write(uint8_t ch) override {
            tmp[tmpPos++] = ch;
            if (tmpPos>MAX_SINGLE_CHARS){
                flush();
            } 
            return 1;
        }

        virtual int availableForWrite() override {
            return DEFAULT_BUFFER_SIZE;
        }

        // removed override because some old implementation did not define this method as virtual
        virtual void flush() FLUSH_OVERRIDE {
            write((const uint8_t*)tmp, tmpPos-1);
            tmpPos=0;
        }

        // overwrite to do something useful
        virtual void setAudioInfo(AudioBaseInfo info) override {
            TRACED();
            cfg = info;
            info.logInfo();
            if (p_notify!=nullptr){
                p_notify->setAudioInfo(info);
            }
        }

        virtual void  setNotifyAudioChange(AudioBaseInfoDependent &bi) override {
            p_notify = &bi;
        }

        /// If true we need to release the related memory
        virtual bool doRelease() {
            return false;
        }

        virtual AudioBaseInfo audioInfo() override {
            return cfg;
        }

        /// Writes n 0 values (= silence)
        /// @param len 
        virtual void writeSilence(size_t len){
            int16_t zero = 0;
            for (int j=0;j<len/2;j++){
            write((uint8_t*)&zero,2);
            } 
        }

    protected:
        uint8_t tmp[MAX_SINGLE_CHARS];
        int tmpPos=0;
        AudioBaseInfoDependent *p_notify=nullptr;
        AudioBaseInfo cfg;

};



/**
 * @brief Stream Wrapper which can be used to print the values as readable ASCII to the screen to be analyzed in the Serial Plotter
 * The frames are separated by a new line. The channels in one frame are separated by a ,
 * @ingroup io
 * @tparam T 
 * @author Phil Schatzmann
 * @copyright GPLv3
*/
template<typename T>
class CsvStream : public AudioPrint {

    public:
        CsvStream(int buffer_size=DEFAULT_BUFFER_SIZE, bool active=true) {
            this->active = active;
        }

        /// Constructor
        CsvStream(Print &out, int channels=2, int buffer_size=DEFAULT_BUFFER_SIZE, bool active=true) {
            this->channels = channels;
            this->out_ptr = &out;
            this->active = active;
        }

        /// Starts the processing with 2 channels
        void begin(){
             TRACED();
            this->active = true;
        }

        /// Provides the default configuration
        AudioBaseInfo defaultConfig(){
            AudioBaseInfo info;
            info.channels = 2;
            info.sample_rate = 44100;
            info.bits_per_sample = sizeof(T)*8;
            return info;
        }

        /// Starts the processing with the number of channels defined in AudioBaseInfo
        bool begin(AudioBaseInfo info){
             TRACED();
            this->active = true;
            this->channels = info.channels;
            return channels!=0;
        }

        /// Starts the processing with the defined number of channels 
        void begin(int channels, Print &out=Serial){
             TRACED();
            this->channels = channels;
            this->out_ptr = &out;
            this->active = true;
        }

        /// Sets the CsvStream as inactive 
        void end() {
             TRACED();
            active = false;
        }

        /// defines the number of channels
        virtual void setAudioInfo(AudioBaseInfo info) {
             TRACEI();
            info.logInfo();
            this->channels = info.channels;
        };

        /// Writes the data - formatted as CSV -  to the output stream
        virtual size_t write(const uint8_t* data, size_t len) {   
            if (!active) return 0;
            TRACED();
            size_t lenChannels = len / (sizeof(T)*channels); 
            data_ptr = (T*)data;
            for (size_t j=0;j<lenChannels;j++){
                for (int ch=0;ch<channels;ch++){
                    if (out_ptr!=nullptr && data_ptr!=nullptr){
                        int value = *data_ptr;
                        out_ptr->print(value);
                    }
                    data_ptr++;
                    if (ch<channels-1) Serial.print(", ");
                }
                Serial.println();
            }
            return len;
        }

        int availableForWrite() {
            return 1024;
        }

    protected:
        T *data_ptr;
        Print *out_ptr = &Serial;
        int channels = 2;
        bool active = false;

};

/**
 * @brief Creates a Hex Dump
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class HexDumpStream : public AudioPrint {

    public:
        HexDumpStream(int buffer_size=DEFAULT_BUFFER_SIZE, bool active=true) {
            this->active = active;
        }

        /// Constructor
        HexDumpStream(Print &out, int buffer_size=DEFAULT_BUFFER_SIZE, bool active=true) {
            this->out_ptr = &out;
            this->active = active;
        }

        void begin(AudioBaseInfo info){
            TRACED();
            info.logInfo();
            this->active = true;
            pos = 0;
        }

        void begin(){
             TRACED();
            this->active = true;
            pos = 0;
        }

        /// Sets the CsvStream as inactive 
        void end() {
             TRACED();
            active = false;
        }

        void flush(){
            Serial.println();
            pos = 0;
        }

        virtual size_t write(const uint8_t* data, size_t len) {   
            if (!active) return 0;
             TRACED();
            for (size_t j=0;j<len;j++){
                out_ptr->print(data[j], HEX);
                out_ptr->print(" ");
                pos++;
                if (pos == 8){
                    Serial.print(" - ");
                }
                if (pos == 16){
                    Serial.println();
                    pos = 0;
                }
            }
            return len;
        }

    protected:
        Print *out_ptr = &Serial;
        int pos = 0;
        bool active = false;
};


/**
 * @brief Wrapper which converts a AudioStream to a AudioPrint
 * @ingroup tools
 */
class AdapterAudioStreamToAudioPrint : public AudioPrint {
    public: 
        AdapterAudioStreamToAudioPrint(AudioStream &stream){
            p_stream = &stream;
        }
        void setAudioInfo(AudioBaseInfo info){
            p_stream->setAudioInfo(info);
        }
        size_t write(const uint8_t *buffer, size_t size){
            return p_stream->write(buffer,size);
        }

        virtual bool doRelease() {
            return true;
        }
       
    protected:
        AudioStream *p_stream=nullptr;
};

/**
 * @brief Wrapper which converts a Print to a AudioPrint
 * @ingroup tools
 */
class AdapterPrintToAudioPrint : public AudioPrint {
    public: 
        AdapterPrintToAudioPrint(Print &print){
            p_print = &print;
        }
        void setAudioInfo(AudioBaseInfo info){
        }
        size_t write(const uint8_t *buffer, size_t size){
            return p_print->write(buffer,size);
        }
        virtual bool doRelease() {
            return true;
        }
    protected:
        Print *p_print=nullptr;
};


/**
 * @brief Replicates the output to multiple destinations.
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MultiOutput : public AudioPrint {
    public:

        /// Defines a MultiOutput with no final output: Define your outputs with add()
        MultiOutput() = default;

        /// Defines a MultiOutput with a single final outputs,
        MultiOutput(AudioPrint &out){
            add(out);            
        }

        MultiOutput(AudioStream &out){
            add(out);            
        }

        /// Defines a MultiOutput with 2 final outputs
        MultiOutput(AudioPrint &out1, AudioPrint &out2){
            add(out1);
            add(out2);
        }

        /// Defines a MultiOutput with 2 final outputs
        MultiOutput(AudioStream &out1, AudioStream &out2){
            add(out1);
            add(out2);
        }

        virtual ~MultiOutput() {
            for (int j=0;j<vector.size();j++){
                if (vector[j]->doRelease()){
                    delete vector[j];
                }
            }
        }

        bool begin(AudioBaseInfo info){
            setAudioInfo(info);
            return true;
        }

        /// Add an additional AudioPrint output
        void add(AudioPrint &out){
            vector.push_back(&out);
        }

        /// Add an AudioStream to the output
        void add(AudioStream &stream){
            AdapterAudioStreamToAudioPrint* out = new AdapterAudioStreamToAudioPrint(stream);
            vector.push_back(out);
        }

        void add(Print &print){
            AdapterPrintToAudioPrint* out = new AdapterPrintToAudioPrint(print);
            vector.push_back(out);
        }

        void flush() {
            for (int j=0;j<vector.size();j++){
                vector[j]->flush();
            }
        }

        void setAudioInfo(AudioBaseInfo info){
            for (int j=0;j<vector.size();j++){
                vector[j]->setAudioInfo(info);
            }
        }

        size_t write(const uint8_t *buffer, size_t size){
            for (int j=0;j<vector.size();j++){
                int open = size;
                int start = 0;
                while(open>0){
                    int written = vector[j]->write(buffer+start, open);
                    open -= written;
                    start += written;
                }
            }
            return size;
        }

        size_t write(uint8_t ch){
            for (int j=0;j<vector.size();j++){
                int open = 1;
                while(open>0){
                    open -= vector[j]->write(ch);
                }
            }
            return 1;
        }

    protected:
        Vector<AudioPrint*> vector;

};

/**
 * @brief Mixing of multiple outputs to one final output
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T 
 */
template<typename T>
class OutputMixer : public Print {
  public:
    OutputMixer(Print &finalOutput, int outputStreamCount) {
      p_final_output = &finalOutput;
      setOutputCount(outputStreamCount);
    };

    void setOutputCount(int count){
      output_count = count;
      buffers.resize(count);
      for (int i=0;i<count;i++){
          buffers[i] = nullptr;
      }
      weights.resize(count);
      for (int i=0;i<count;i++){
          weights[i] = 1.0;
      }
      
      update_total_weights();
    }

    /// Defines a new weight for the indicated channel: If you set it to 0 it is muted.
    void setWeight(int channel, float weight){
      if (channel<size()){
        weights[channel] = weight;
      } else {
        LOGE("Invalid channel %d - max is %d", channel, size()-1);
      }
      update_total_weights();
    }

    void begin(int copy_buffer_size_bytes=DEFAULT_BUFFER_SIZE, MemoryType memoryType=PS_RAM) {
      is_active = true;
      size_bytes = copy_buffer_size_bytes;
      stream_idx = 0;
      memory_type = memoryType;
      allocate_buffers();
    }

    /// Remove all input streams
    void end() {
      total_weights = 0.0;
      is_active = false;
      // release memory for buffers
      free_buffers();
    }

    /// Number of stremams to which are mixed together
    int size() {
      return output_count;
    }

    size_t write(uint8_t) override{
        return 0;
    }

    /// Write the data from a simgle stream which will be mixed together (the stream idx is increased)
    size_t write(const uint8_t *buffer_c, size_t bytes) override {
        size_t result = write(stream_idx, buffer_c, bytes);
        // after writing the last stream we flush
        stream_idx++;
        if (stream_idx>=output_count){
            flushMixer();
        }
        return result;
    }

    /// Write the data for an individual stream idx which will be mixed together 
    size_t write(int idx, const uint8_t *buffer_c, size_t bytes)  {
        LOGD("write idx %d: %d",stream_idx, bytes);
        size_t result = 0;
        RingBuffer<T>* p_buffer = idx<output_count ? buffers[idx] : nullptr;
        assert(p_buffer!=nullptr);
        size_t samples = bytes/sizeof(T);
        if (p_buffer->availableForWrite()<samples){
            LOGW("Available Buffer too small %d: requested: %d -> increase the buffer size", p_buffer->availableForWrite(),  samples);
        } else {
            result = p_buffer->writeArray((T*)buffer_c, samples) * sizeof(T);
        }
        return result;
    }
    
    /// Provides the bytes available to write for the current stream buffer
    int availableForWrite() override { return is_active ? availableForWrite(stream_idx) : 0; }

    /// Provides the bytes available to write for the indicated stream index
    int availableForWrite(int idx) { 
        RingBuffer<T>* p_buffer = buffers[idx];
        if (p_buffer == nullptr) return 0;
        return p_buffer->availableForWrite();
     }

    /// Force output to final destination
    void flushMixer() {
        LOGD("flush");
        bool result = false;

        // determine ringbuffer with mininum available data
        size_t samples = size_bytes/sizeof(T);
        for (int j=0;j<output_count;j++){
            samples = MIN(samples, (size_t) buffers[j]->available());            
        }

        if (samples>0){
            result = true;
            // mix data from ringbuffers to output
            output.resize(samples);
            memset(output.data(),0, samples*sizeof(T));
            for (int j=0;j<output_count;j++) {
                float weight = weights[j];
                // sum up input samples to result samples 
                for (int i=0;i<samples; i++){
                    output[i] += weight * buffers[j]->read() / total_weights;
                }
            }

            // write output
            LOGD("write to final out: %d", samples*sizeof(T));
            p_final_output->write((uint8_t*)output.data(), samples*sizeof(T));
        }
        stream_idx = 0;
        return;
    }

  protected:
    Vector<RingBuffer<T>*> buffers{0};
    Vector<T> output{0};
    Vector<float> weights{0}; 
    Print *p_final_output=nullptr;
    float total_weights = 0.0;
    bool is_active = false;
    int stream_idx = 0;
    int size_bytes;
    int output_count;
    MemoryType memory_type;
    void *p_memory=nullptr;

    void update_total_weights() {
        total_weights = 0.0;
        for (int j=0;j<weights.size();j++){
            total_weights += weights[j];
        }
    }

    void allocate_buffers() {
      // allocate ringbuffers for each output
      for (int j=0;j<output_count;j++){
        if (buffers[j]!=nullptr){
            delete buffers[j];
        }
#ifdef ESP32
        if (memory_type==PS_RAM && ESP.getFreePsram()>=size_bytes){
            p_memory = ps_malloc(size_bytes);
            LOGI("Buffer %d allocated %d bytes in PS_RAM",j, size_bytes);
        } else {
            p_memory = malloc(size_bytes);
            LOGI("Buffer %d allocated %d bytes in RAM",j, size_bytes);
        }
        if (p_memory!=nullptr){
            buffers[j] = new(p_memory) RingBuffer<T>(size_bytes/sizeof(T));
        } else {
            LOGE("Not enough memory to allocate %d bytes",size_bytes);
        }
#else
        buffers[j] = new RingBuffer<T>(size_bytes/sizeof(T));
#endif
      }
    }

    void free_buffers() {
      // allocate ringbuffers for each output
      for (int j=0;j<output_count;j++){
        if (buffers[j]!=nullptr){
            delete buffers[j];
#ifdef ESP32
            if (p_memory!=nullptr){
                free(p_memory);
            }
#endif
            buffers[j] = nullptr;
        }
      }
    }

};


/**
 * @brief A simple class to determine the volume
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VolumePrint : public AudioPrint {
    public:
        VolumePrint() = default;

        ~VolumePrint() {
            if (volumes!=nullptr) delete volumes;
            if (volumes_tmp!=nullptr) delete volumes_tmp;
        }
     
        bool begin(AudioBaseInfo info){
            setAudioInfo(info);
            return true;
        }

        void setAudioInfo(AudioBaseInfo info){
            this->info = info;
            if (info.channels){
                volumes = new float[info.channels];
                volumes_tmp = new float[info.channels];
            }
        }

        size_t write(const uint8_t *buffer, size_t size){
            for (int j=0;j<info.channels;j++){
                volumes_tmp[j]=0;
            }
            switch(info.bits_per_sample){
                case 16: {
                        int16_t *buffer16 = (int16_t*)buffer;
                        int samples16 = size/2;
                        for (int j=0;j<samples16;j++){
                            float tmp = static_cast<float>(abs(buffer16[j]));
                            updateVolume(tmp,j);
                        }
                        commit();
                    } break;
                case 32: {
                        int32_t *buffer32 = (int32_t*)buffer;
                        int samples32 = size/4;
                        for (int j=0;j<samples32;j++){
                            float tmp = static_cast<float>(abs(buffer32[j]))/samples32;
                            updateVolume(tmp,j);
                        }
                        commit();
                    }break;

                default:
                    LOGE("Unsupported bits_per_sample: %d", info.bits_per_sample);
                    f_volume = 0;
                    break;
            }
            return size;
        }

        /// Determines the volume (the range depends on the bits_per_sample)
        float volume() {
            return f_volume;
        }

        /// Determines the volume for the indicated channel
        float volume(int channel) {
            return channel<info.channels ? volumes[channel]:0.0;
        }

    protected:
        AudioBaseInfo info;
        float f_volume_tmp = 0;
        float f_volume = 0;
        float *volumes=nullptr;
        float *volumes_tmp=nullptr;

        void updateVolume(float tmp, int j) {
            if (tmp>f_volume){
                f_volume_tmp = tmp;
            }
            if (volumes_tmp!=nullptr && tmp>volumes_tmp[j%info.channels]){
                volumes_tmp[j%info.channels] = tmp;
            }
        }

        void commit(){
            f_volume = f_volume_tmp;
            for (int j=0;j<info.channels;j++){
                volumes[j] = volumes_tmp[j];
            }
        }
};

/**
 * @brief Prints to a preallocated memory
 * @ingroup io
 */
class MemoryPrint : public AudioPrint {
    public:
        MemoryPrint(uint8_t*start, int len){
            p_start = start;
            p_next = start;
            max_size = len;
            if (p_next==nullptr){
                LOGE("start must not be null");
            }
        }

        size_t write(const uint8_t *buffer, size_t len) override {
            if (p_next==nullptr) return 0;
            if (pos+len<=max_size){
                memcpy(p_next, buffer,len);
                pos+=len;
                p_next+=len;
                return len;
            } else {
                LOGE("Buffer too small: pos:%d, size: %d ", pos, (int)max_size);
                return 0;
            }
        }

        int availableForWrite() override {
            return max_size-pos;
        }

        int size() {
            return max_size;
        }
        
    protected:
        int pos=0;
        uint8_t* p_start=nullptr;
        uint8_t *p_next=nullptr;
        size_t max_size;

};

} //n namespace