
#pragma once
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioPrint.h"
#include "AudioTools/VolumeControl.h"

namespace audio_tools {

/**
 * @brief Config for VolumeStream
 * @author Phil Schatzmann
 * @copyright GPLv3 
 */
struct VolumeStreamConfig : public AudioBaseInfo {
  VolumeStreamConfig(){
    bits_per_sample = 16;
    channels = 2;
  }
  bool allow_boost = false;
  float volume=1.0;  // start_volume
};


/**
 * @brief Adjust the volume of the related input or output: To work properly the class needs to know the 
 * bits per sample and number of channels!
 * AudioChanges are forwareded to the related Print or Stream class.
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class VolumeStream : public AudioStreamX {
    public:
        /// Default Constructor
        VolumeStream() = default;

        /// Destructor
        ~VolumeStream() {
            cleanup();
        };

        /// Constructor which assigns Print output
        VolumeStream(Print &out) {
            setTarget(out);
        }

        /// Constructor which assigns Stream input or output
        VolumeStream(Stream &in) {
            setTarget(in);
        }

        /// Constructor which assigns Print output
        VolumeStream(AudioPrint &out) {
            Print *p_print = &out;
            setTarget(*p_print);
            p_notify = (AudioBaseInfoDependent*) &out;
        }

        /// Constructor which assigns Stream input or output
        VolumeStream(AudioStream &io) {
            Stream *p_stream = &io;
            setTarget(*p_stream);
            p_notify = (AudioBaseInfoDependent *)&io;
        }

        void setTarget(Print &out){
            p_out = &out;
        }
        
        void setTarget(Stream &in){
            p_in = &in;
            p_out = p_in;
        }

        VolumeStreamConfig defaultConfig() {
            VolumeStreamConfig c;
            return c;
        }

        bool begin(AudioBaseInfo cfg)  {
          VolumeStreamConfig cfg1;
          cfg1.channels = cfg.channels;
          cfg1.sample_rate = cfg.sample_rate;
          cfg1.bits_per_sample = cfg.bits_per_sample;
          // keep volume which might habe been defined befor calling begin
          cfg1.volume = info.volume;  
          return begin(cfg1);
        }

        void end() override {
            is_active = false;
        }

        /// starts the processing 
        bool begin(VolumeStreamConfig cfg){
            TRACED();
            info = cfg;
            max_value = NumberConverter::maxValue(info.bits_per_sample);
            if (info.channels>max_channels){
              max_channels = info.channels;
            }

            // usually we use a exponential volume control - except if we allow values > 1.0
            if (cfg.allow_boost){
              setVolumeControl(linear_vc);
            } else {
              setVolumeControl(pot_vc);
            }

            // set start volume
            setVolume(cfg.volume); 

            return true;
        }

        /// Defines the volume control logic
        void setVolumeControl(VolumeControl &vc){
            cached_volume.setVolumeControl(vc);
        }

        /// Resets the volume control to use the standard logic
        void resetVolumeControl(){
            cached_volume.setVolumeControl(pot_vc);
        }

        /// Read raw PCM audio data, which will be the input for the volume control 
        virtual size_t readBytes(uint8_t *buffer, size_t length) override { 
            TRACED();
            if (buffer==nullptr || p_in==nullptr){
                LOGE("NPE");
                return 0;
            }
            size_t result = p_in->readBytes(buffer, length);
            if (is_active) applyVolume(buffer, result);
            return result;
        }

        /// Writes raw PCM audio data, which will be the input for the volume control 
        virtual size_t write(const uint8_t *buffer, size_t size) override {
            TRACED();
            if (buffer==nullptr || p_out==nullptr){
                LOGE("NPE");
                return 0;
            }
            if (is_active) applyVolume(buffer,size);
            return p_out->write(buffer, size);
        }

        /// Provides the nubmer of bytes we can write
        virtual int availableForWrite() override { 
            return p_out==nullptr? 0 : p_out->availableForWrite();
        }

        /// Provides the nubmer of bytes we can write
        virtual int available() override { 
            return p_in==nullptr? 0 : p_in->available();
        }

        /// Detines the Audio info - The bits_per_sample are critical to work properly!
        void setAudioInfo(AudioBaseInfo cfg) override {
            TRACED();
            // pass on notification
            if (p_notify!=nullptr){
              p_notify->setAudioInfo(cfg);
            }
            begin(cfg);
        }

        /// Defines the volume for all channels:  needs to be in the range of 0 to 1.0
        void setVolume(float vol){
            // just to make sure that we have a valid start volume before begin
            info.volume = vol; 
            for (int j=0;j<info.channels;j++){
              setVolume(vol, j);
            }
        }

        /// Sets the volume for one channel
        void setVolume(float vol, int channel){
            if (channel<info.channels){
              setup(vol);
              float volume_value = volumeValue(vol);
              LOGI("setVolume: %f", volume_value);
              float factor = volumeControl().getVolumeFactor(volume_value);
              factor_for_channel[channel]=factor;
            } else {
              LOGE("Invalid channel %d - max: %d", channel, info.channels-1);
            }
        }

        /// Provides the current volume setting
        float volume() {
            return volume_values[0];
        }

        /// Provides the current volume setting for the indicated channel
        float volume(int channel) {
            return channel>=info.channels? 0 : volume_values[channel];
        }

    protected:
        Print *p_out=nullptr;
        Stream *p_in=nullptr;
        VolumeStreamConfig info;
        LinearVolumeControl linear_vc{true};
        SimulatedAudioPot pot_vc;
        CachedVolumeControl cached_volume{pot_vc};
        float *volume_values = nullptr;
        float *factor_for_channel = nullptr;
        bool is_active = false;
        float max_value = 32767; // max value for clipping
        int max_channels=0;

        void setup(float vol) {
            is_active = vol!=1.0f;
            if (info.channels>max_channels){
              cleanup();
            }
            if (factor_for_channel==nullptr){
              factor_for_channel = new float[info.channels];
            }
            if (volume_values==nullptr){
              volume_values = new float[info.channels];
            }
        }

        void cleanup() {
          if (factor_for_channel!=nullptr) {
            delete []factor_for_channel;
            factor_for_channel = nullptr;
          } 
          if (volume_values!=nullptr) {
            delete []volume_values;
            volume_values = nullptr;
          }
        }

        float volumeValue(float vol){
            if (!info.allow_boost && vol>1.0f) vol = 1.0;
            if (vol<0.0f) vol = 0.0;

            // round to 2 digits
            float value = (int)(vol * 100 + .5f);
            float volume_value = (float)value / 100;
            return volume_value;
        }

        VolumeControl &volumeControl(){
            return cached_volume;
        }

        float factorForChannel(int channel){
            return factor_for_channel==nullptr ? 1.0 : factor_for_channel[channel];
        }

        void applyVolume(const uint8_t *buffer, size_t size){
            switch(info.bits_per_sample){
                case 16:
                    applyVolume16((int16_t*)buffer, size/2);
                    break;
                case 24:
                    applyVolume24((int24_t*)buffer, size/3);
                    break;
                case 32:
                    applyVolume32((int32_t*)buffer, size/4);
                    break;
                default:
                    LOGE("Unsupported bits_per_sample: %d", info.bits_per_sample);
            }
        }

        void applyVolume16(int16_t* data, size_t size){
            for (size_t j=0;j<size;j++){
                float result = factorForChannel(j%info.channels) * data[j];
                if (!info.allow_boost){
                    if (result>max_value) result = max_value;
                    if (result<-max_value) result = -max_value;
                } 
                data[j]= static_cast<int16_t>(result);
            }
        }

        void applyVolume24(int24_t* data, size_t size) {
            for (size_t j=0;j<size;j++){
                float result = factorForChannel(j%info.channels) * data[j];
                if (!info.allow_boost){
                    if (result>max_value) result = max_value;
                    if (result<-max_value) result = -max_value;
                } 
                int32_t result1 = result;
                data[j]= static_cast<int24_t>(result1);
            }
        }

        void applyVolume32(int32_t* data, size_t size) {
            for (size_t j=0;j<size;j++){
                float result = factorForChannel(j%info.channels) * data[j];
                if (!info.allow_boost){
                    if (result>max_value) result = max_value;
                    if (result<-max_value) result = -max_value;
                } 
                data[j]= static_cast<int32_t>(result);
            }
        }
};

}
