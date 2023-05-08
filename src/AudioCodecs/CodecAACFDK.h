#pragma once

#include "AudioCodecs/AudioEncoded.h"
#include "AACDecoderFDK.h"
#include "AACEncoderFDK.h"


namespace audio_tools {

// audio change notification target
AudioInfoSupport *audioChangeFDK = nullptr;

/**
 * @brief Audio Decoder which decodes AAC into a PCM stream
 * This is basically just a wrapper using https://github.com/pschatzmann/arduino-fdk-aac
 * which uses AudioInfo and provides the handlig of AudioInfo changes.
 * @ingroup codecs
 * @ingroup decoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AACDecoderFDK : public AudioDecoder  {
    public:
        AACDecoderFDK(){
            TRACED();
            dec = new aac_fdk::AACDecoderFDK();
        }

        AACDecoderFDK(Print &out_stream, int output_buffer_size=2048){
            TRACED();
            dec = new aac_fdk::AACDecoderFDK(out_stream, output_buffer_size);
        }

        virtual ~AACDecoderFDK(){
            delete dec;
        }

        /// Defines the output stream
        void setOutput(Print &out_stream){
            dec->setOutput(out_stream);
        }

        void begin(){
            dec->begin(TT_MP4_ADTS, 1);
        }

        // opens the decoder
        void begin(TRANSPORT_TYPE transportType, UINT nrOfLayers){
            dec->begin(transportType, nrOfLayers);
        }

        /**
         * @brief Explicitly configure the decoder by passing a raw AudioSpecificConfig (ASC) or a StreamMuxConfig
         * (SMC), contained in a binary buffer. This is required for MPEG-4 and Raw Packets file format bitstreams
         * as well as for LATM bitstreams with no in-band SMC. If the transport format is LATM with or without
         * LOAS, configuration is assumed to be an SMC, for all other file formats an ASC.
         * 
         **/
        AAC_DECODER_ERROR configure(uint8_t *conf, const uint32_t &length) {
            return dec->configure(conf, length);
        }

        // write AAC data to be converted to PCM data
        virtual size_t write(const void *in_ptr, size_t in_size) {
            return dec->write(in_ptr, in_size);
        }

        // provides detailed information about the stream
        CStreamInfo audioInfoEx(){
            return dec->audioInfo();
        }

        // provides common information
        AudioInfo audioInfo() {
            AudioInfo result;
            CStreamInfo i = audioInfoEx();
            result.channels = i.numChannels;
            result.sample_rate = i.sampleRate;
            result.bits_per_sample = 16;
            return result;
        }

        // release the resources
        void end(){
             TRACED();
            dec->end();
        }

        virtual operator bool() {
            return (bool)*dec;
        }

        aac_fdk::AACDecoderFDK *driver() {
            return dec;
        }

        static void audioChangeCallback(CStreamInfo &info){
            if (audioChangeFDK!=nullptr){
                AudioInfo base;
                base.channels = info.numChannels;
                base.sample_rate = info.sampleRate;
                base.bits_per_sample = 16;
                // notify audio change
                audioChangeFDK->setAudioInfo(base);
            }
        }

        virtual void setNotifyAudioChange(AudioInfoSupport &bi) {
            audioChangeFDK = &bi;
            // register audio change handler
            dec->setInfoCallback(audioChangeCallback);
        }

    protected:
        aac_fdk::AACDecoderFDK *dec=nullptr;
};


/**
 * @brief Encodes PCM data to the AAC format and writes the result to a stream
 * This is basically just a wrapper using https://github.com/pschatzmann/arduino-fdk-aac
 * @ingroup codecs
 * @ingroup encoder
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AACEncoderFDK : public AudioEncoder {

public:

    AACEncoderFDK(){
        enc = new aac_fdk::AACEncoderFDK();
    }

    AACEncoderFDK(Print &out_stream){
        enc = new aac_fdk::AACEncoderFDK();
        enc->setOutput(out_stream);
    }

     ~AACEncoderFDK(){
         delete enc;
     }

     void setOutput(Print &out_stream){
         enc->setOutput(out_stream);
     }

    /*!< Total encoder bitrate. This parameter is    
                mandatory and interacts with ::AACENC_BITRATEMODE.
                - CBR: Bitrate in bits/second.
                - VBR: Variable bitrate. Bitrate argument will
                be ignored. See \ref suppBitrates for details. */    
    virtual void setBitrate(int bitrate){
        enc->setBitrate(bitrate);
    }

    /*!< Audio object type. See ::AUDIO_OBJECT_TYPE in FDK_audio.h.
                   - 2: MPEG-4 AAC Low Complexity.
                   - 5: MPEG-4 AAC Low Complexity with Spectral Band Replication
                 (HE-AAC).
                   - 29: MPEG-4 AAC Low Complexity with Spectral Band
                 Replication and Parametric Stereo (HE-AAC v2). This
                 configuration can be used only with stereo input audio data.
                   - 23: MPEG-4 AAC Low-Delay.
                   - 39: MPEG-4 AAC Enhanced Low-Delay. Since there is no
                 ::AUDIO_OBJECT_TYPE for ELD in combination with SBR defined,
                 enable SBR explicitely by ::AACENC_SBR_MODE parameter. The ELD
                 v2 212 configuration can be configured by ::AACENC_CHANNELMODE
                 parameter.
                   - 129: MPEG-2 AAC Low Complexity.
                   - 132: MPEG-2 AAC Low Complexity with Spectral Band
                 Replication (HE-AAC).

                   Please note that the virtual MPEG-2 AOT's basically disables
                 non-existing Perceptual Noise Substitution tool in AAC encoder
                 and controls the MPEG_ID flag in adts header. The virtual
                 MPEG-2 AOT doesn't prohibit specific transport formats. */
    virtual void setAudioObjectType(int aot){
        enc->setAudioObjectType(aot);
    }

    /*!< This parameter controls the use of the afterburner feature.
                   The afterburner is a type of analysis by synthesis algorithm
                 which increases the audio quality but also the required
                 processing power. It is recommended to always activate this if
                 additional memory consumption and processing power consumption
                   is not a problem. If increased MHz and memory consumption are
                 an issue then the MHz and memory cost of this optional module
                 need to be evaluated against the improvement in audio quality
                 on a case by case basis.
                   - 0: Disable afterburner (default).
                   - 1: Enable afterburner. */
    virtual void setAfterburner(bool afterburner){
        enc->setAfterburner(afterburner);
    }

    /*!< Configure SBR independently of the chosen Audio
                Object Type ::AUDIO_OBJECT_TYPE. This parameter
                is for ELD audio object type only.
                    - -1: Use ELD SBR auto configurator (default).
                    - 0: Disable Spectral Band Replication.
                    - 1: Enable Spectral Band Replication. */    
    virtual void setSpectralBandReplication(int eld_sbr){
        enc->setSpectralBandReplication(eld_sbr);
    }

     /*!< Bitrate mode. Configuration can be different
                kind of bitrate configurations:
                - 0: Constant bitrate, use bitrate according
                to ::AACENC_BITRATE. (default) Within none
                LD/ELD ::AUDIO_OBJECT_TYPE, the CBR mode makes
                use of full allowed bitreservoir. In contrast,
                at Low-Delay ::AUDIO_OBJECT_TYPE the
                bitreservoir is kept very small.
                - 1: Variable bitrate mode, \ref vbrmode
                "very low bitrate".
                - 2: Variable bitrate mode, \ref vbrmode
                "low bitrate".
                - 3: Variable bitrate mode, \ref vbrmode
                "medium bitrate".
                - 4: Variable bitrate mode, \ref vbrmode
                "high bitrate".
                - 5: Variable bitrate mode, \ref vbrmode
                "very high bitrate". */    
    virtual void setVariableBitrateMode(int vbr){
        enc->setVariableBitrateMode(vbr);
    }
    
    /**
     * @brief Set the Output Buffer Size object
     * 
     * @param outbuf_size 
     */
    virtual void setOutputBufferSize(int outbuf_size){
        enc->setOutputBufferSize(outbuf_size);
    }

    /// Defines the Audio Info
    virtual void setAudioInfo(AudioInfo from) {
        TRACED();
        aac_fdk::AudioInfo info;
        info.channels = from.channels;
        info.sample_rate = from.sample_rate;
        info.bits_per_sample = from.bits_per_sample;
        enc->setAudioInfo(info);
    }

    /**
     * @brief Opens the encoder  
     * 
     * @param info 
     * @return int 
     */
    virtual void begin(AudioInfo info) {
        TRACED();
        enc->begin(info.channels,info.sample_rate, info.bits_per_sample);
    }

    /**
     * @brief Opens the encoder  
     * 
     * @param input_channels 
     * @param input_sample_rate 
     * @param input_bits_per_sample 
     * @return int 0 => ok; error with negative number
     */
    virtual void begin(int input_channels=2, int input_sample_rate=44100, int input_bits_per_sample=16) {
        TRACED();
        enc->begin(input_channels,input_sample_rate, input_bits_per_sample);
    }

    // starts the processing
    void begin() {
        enc->begin();
    }
    
    // convert PCM data to AAC
    size_t write(const void *in_ptr, size_t in_size){
        LOGD("write %d bytes", (int)in_size);
        return enc->write((uint8_t*)in_ptr, in_size);
    }

    // release resources
    void end(){
        TRACED();
        enc->end();
    }

    UINT getParameter(const AACENC_PARAM param) {
        return enc->getParameter(param);
    }
    
    int setParameter(AACENC_PARAM param, uint32_t value){
        return enc->setParameter(param, value);
    }

    aac_fdk::AACEncoderFDK *driver() {
        return enc;
    }

    const char *mime() {
        return "audio/aac";
    }

    operator bool(){
        return (bool) *enc;
    }


protected:
    aac_fdk::AACEncoderFDK *enc=nullptr;

};

}

