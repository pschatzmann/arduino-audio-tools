#pragma once

#include "AAC.h"
#include "AudioTools.h"

namespace audio_tools {

/**
 * @brief Audio Decoder which decodes AAC into a PCM stream
 * 
 */
class AACDecoder : public AudioWriter  {
    public:
        AACDecoder(Stream &out_stream, int output_buffer_size=2048){
            this->out = &out_stream;
            this->output_buffer_size = output_buffer_size;
            this->output_buffer = new INT_PCM[output_buffer_size];
        }

        ~AACDecoder(){
            close();
        }

        // opens the decoder
        int begin(TRANSPORT_TYPE transportType = TT_UNKNOWN, UINT nrOfLayers=1){
            aacDecoderInfo = aacDecoder_Open(transportType, nrOfLayers);
			if (aacDecoderInfo==NULL){
				LOGE("aacDecoder_Open -> Error");
				return -1;
			}
			is_open = true;
            return 0;
        }

        /**
         * @brief Explicitly configure the decoder by passing a raw AudioSpecificConfig (ASC) or a StreamMuxConfig
         * (SMC), contained in a binary buffer. This is required for MPEG-4 and Raw Packets file format bitstreams
         * as well as for LATM bitstreams with no in-band SMC. If the transport format is LATM with or without
         * LOAS, configuration is assumed to be an SMC, for all other file formats an ASC.
         * 
         **/
        AAC_DECODER_ERROR configure(uint8_t *conf, const uint32_t &length) {
            return aacDecoder_ConfigRaw (aacDecoderInfo, &conf, &length );
        }

        // write AAC data to be converted to PCM data
      	virtual size_t write(const void *in_ptr, size_t in_size) {
			size_t result = 0;
			if (aacDecoderInfo!=nullptr) {
				uint32_t bytesValid = 0;
				AAC_DECODER_ERROR error = aacDecoder_Fill(aacDecoderInfo, (UCHAR **)&in_ptr, (const UINT*)&in_size, &bytesValid); 
				if (error == AAC_DEC_OK) {
					int flags = 0;
					error = aacDecoder_DecodeFrame(aacDecoderInfo, output_buffer, output_buffer_size, flags); 
					// write pcm to output stream
					if (error == AAC_DEC_OK){
						result = out->write((uint8_t*)output_buffer, output_buffer_size*sizeof(INT_PCM));
					}
					// if not all bytes were used we process them now
					if (bytesValid<in_size){
						const uint8_t *start = static_cast<const uint8_t*>(in_ptr)+bytesValid;
						uint32_t act_len = in_size-bytesValid;
						aacDecoder_Fill(aacDecoderInfo, (UCHAR**) &start, &act_len, &bytesValid);
					}
				}
			}
            return result;
        }

        // provides information about the stream
        CStreamInfo &info(){
            return *aacDecoder_GetStreamInfo(aacDecoderInfo);
        }

        // release the resources
        void close(){
            if (aacDecoderInfo!=nullptr){
                aacDecoder_Close(aacDecoderInfo); 
                aacDecoderInfo = nullptr;
            }
            if (output_buffer!=nullptr){
                delete[] output_buffer;
                output_buffer = nullptr;
            }
			is_open = false;
        }

       virtual operator boolean() {
		   return is_open;
	   }

    protected:
        Stream *out = nullptr;
		bool is_cleanup_stream = false;
        HANDLE_AACDECODER aacDecoderInfo;
        int output_buffer_size;
        INT_PCM* output_buffer = nullptr;
		bool is_open;
};


/**
 * @brief Encodes PCM data to the AAC format and writes the result to a stream
 * 
 */
class AACEncoder : public AudioWriter {

public:
	AACEncoder(Stream &out_stream){
		this->out = &out_stream;
	}

	 ~AACEncoder(){
		 close();
	 }

	/*!< Total encoder bitrate. This parameter is	
				mandatory and interacts with ::AACENC_BITRATEMODE.
				- CBR: Bitrate in bits/second.
				- VBR: Variable bitrate. Bitrate argument will
				be ignored. See \ref suppBitrates for details. */	
	virtual void setBitrate(int bitrate){
		this->bitrate = bitrate;
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
		this->aot = aot;
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
		this->afterburner = afterburner;
	}

	/*!< Configure SBR independently of the chosen Audio
				Object Type ::AUDIO_OBJECT_TYPE. This parameter
				is for ELD audio object type only.
					- -1: Use ELD SBR auto configurator (default).
					- 0: Disable Spectral Band Replication.
					- 1: Enable Spectral Band Replication. */	
	virtual void setSpecialBandReplication(int eld_sbr){
		this->eld_sbr = eld_sbr;
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
		this->vbr = vbr;
	}
	
	/**
	 * @brief Set the Output Buffer Size object
	 * 
	 * @param outbuf_size 
	 */
	virtual void setOutputBufferSize(int outbuf_size){
		this->out_size = outbuf_size;
	}

	/**
	 * @brief Opens the encoder  
	 * 
	 * @param info 
	 * @return int 
	 */
	virtual int begin(AudioBaseInfo info) {
		begin(info.channels, info.sample_rate, info.bits_per_sample);
	}

	/**
	 * @brief Opens the encoder  
	 * 
	 * @param input_channels 
	 * @param input_sample_rate 
	 * @param input_bits_per_sample 
	 * @return int 0 => ok; error with negative number
	 */
	virtual int begin(int input_channels=2, int input_sample_rate=44100, int input_bits_per_sample=16) {
		this->channels = input_channels;
		this->sample_rate = input_sample_rate;
		this->bits_per_sample = input_bits_per_sample;

		switch (channels) {
		case 1: mode = MODE_1;       break;
		case 2: mode = MODE_2;       break;
		case 3: mode = MODE_1_2;     break;
		case 4: mode = MODE_1_2_1;   break;
		case 5: mode = MODE_1_2_2;   break;
		case 6: mode = MODE_1_2_2_1; break;
		default:
			LOGE("Unsupported WAV channels\n");
			return -1;
		}

		if (aacEncOpen(&handle, 0, channels) != AACENC_OK) {
			LOGE("Unable to open encoder\n");
			return -1;
		}

		if (updateParams()<0) {
			LOGE("Unable to update parameters\n");
			return -1;
		}

		if (aacEncEncode(handle, NULL, NULL, NULL, NULL) != AACENC_OK) {
			LOGE("Unable to initialize the encoder\n");
			return -1;
		}

		if (aacEncInfo(handle, &info) != AACENC_OK) {
			LOGE("Unable to get the encoder info\n");
			return -1;
		}

		input_size = channels*2*info.frameLength;
		input_buf = (uint8_t*) malloc(input_size);
		if (input_buf==nullptr){
			LOGE("Unable to allocate memory for input buffer\n");
			return -1;
		}
		convert_buf = (int16_t*) malloc(input_size);
		if (convert_buf==nullptr){
			LOGE("Unable to allocate memory for convert buffer\n");
			return -1;
		}
		outbuf = (uint8_t*) malloc(out_size);
		if (outbuf==nullptr){
			LOGE("Unable to allocate memory for output buffer\n");
			return -1;
		}

		return 0;
	}
	

	// convert PCM data to AAC
	int32_t write(void *in_ptr, int in_size){
		if (input_buf==nullptr){
			LOGE("The encoder is not open\n");
			return -1;
		}
		in_elem_size = 2;

		in_args.numInSamples = in_size <= 0 ? -1 : in_size / 2;
		in_buf.numBufs = 1;
		in_buf.bufs = &in_ptr;
		in_buf.bufferIdentifiers = &in_identifier;
		in_buf.bufSizes = &in_size;
		in_buf.bufElSizes = &in_elem_size;

		out_ptr = outbuf;
		out_elem_size = 1;
		out_buf.numBufs = 1;
		out_buf.bufs = &out_ptr;
		out_buf.bufferIdentifiers = &out_identifier;
		out_buf.bufSizes = &out_size;
		out_buf.bufElSizes = &out_elem_size;

		if ((err = aacEncEncode(handle, &in_buf, &out_buf, &in_args, &out_args)) != AACENC_OK) {
			// error
			if (err != AACENC_ENCODE_EOF)
				LOGE("Encoding failed\n");
			return -1;
		}

		// no output
		if (out_args.numOutBytes == 0)
			return -2;

		// output to Arduino Stream	
		return out->write((uint8_t*)outbuf, out_args.numOutBytes);
	}

	// release resources
	void close(){
		if (input_buf!=nullptr)
			free(input_buf);
		input_buf = nullptr;

		if (convert_buf!=nullptr)
			free(convert_buf);
		convert_buf = nullptr;

		if (outbuf!=nullptr){
			free(outbuf);
		}
		outbuf=nullptr;

		aacEncClose(&handle);
	}

	UINT getParameter(const AACENC_PARAM param) {
		return aacEncoder_GetParam(handle, param);
	}

	int setParameter(AACENC_PARAM param, uint32_t value){
		return aacEncoder_SetParam(handle, param, value);
	}

protected:
	// common variables
	Stream *out;
	int vbr = 0;
	int bitrate = 64000;
	int ch;
	const char *infile;
	void *wav;
	int format, sample_rate, channels, bits_per_sample;
	int input_size;
	uint8_t* input_buf = nullptr;
	int16_t* convert_buf = nullptr;
	int aot = 2;
	bool afterburner = true;
	int eld_sbr = 0;
	HANDLE_AACENCODER handle;
	CHANNEL_MODE mode;
	AACENC_InfoStruct info = { 0 };
	// loop variables
	AACENC_BufDesc in_buf = { 0 }, out_buf = { 0 };
	AACENC_InArgs in_args = { 0 };
	AACENC_OutArgs out_args = { 0 };
	int in_identifier = IN_AUDIO_DATA;
	int in_elem_size;
	int out_identifier = OUT_BITSTREAM_DATA;
	int out_elem_size;
	void *in_ptr, *out_ptr;
	uint8_t* outbuf;
	int out_size = 20480;
	AACENC_ERROR err;


	int updateParams() {
		if (setParameter(AACENC_AOT, aot) != AACENC_OK) {
			LOGE("Unable to set the AOT\n");
			return -1;
		}
		if (aot == 39 && eld_sbr) {
			if (setParameter(AACENC_SBR_MODE, 1) != AACENC_OK) {
				LOGE("Unable to set SBR mode for ELD\n");
				return -1;
			}
		}
		if (setParameter(AACENC_SAMPLERATE, sample_rate) != AACENC_OK) {
			LOGE("Unable to set the AOT\n");
			return -1;
		}
		if (setParameter(AACENC_CHANNELMODE, mode) != AACENC_OK) {
			LOGE("Unable to set the channel mode\n");
			return -1;
		}
		if (setParameter(AACENC_CHANNELORDER, 1) != AACENC_OK) {
			LOGE("Unable to set the wav channel order\n");
			return -1;
		}
		if (vbr) {
			if (setParameter(AACENC_BITRATEMODE, vbr) != AACENC_OK) {
				LOGE("Unable to set the VBR bitrate mode\n");
				return -1;
			}
		} else {
			if (setParameter(AACENC_BITRATE, bitrate) != AACENC_OK) {
				LOGE("Unable to set the bitrate\n");
				return -1;
			}
		}
		if (setParameter(AACENC_TRANSMUX, TT_MP4_ADTS) != AACENC_OK) {
			LOGE("Unable to set the ADTS transmux\n");
			return -1;
		}
		if (setParameter(AACENC_AFTERBURNER, afterburner) != AACENC_OK) {
			LOGE("Unable to set the afterburner mode\n");
			return -1;
		}
		return 0;
	}

};

}
