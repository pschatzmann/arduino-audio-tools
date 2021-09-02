#pragma once

#include "AudioTools/AudioTypes.h"
#include "MP3EncoderLAME.h"

namespace audio_tools {

/**
 * @brief LAME parameters
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct AudioInfoLAME :  AudioBaseInfo  {
    AudioInfoLAME () = default;
    AudioInfoLAME (const AudioInfoLAME  &) = default;

	int quality = 7; // 0..9.  0=best (very slow).  9=worst.
};

/**
 * @brief Encodes PCM data to the MP3 format and writes the result to a stream
 * This is basically just a wrapper using https://github.com/pschatzmann/arduino-liblame
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class MP3EncoderLAME : public AudioEncoder {

public:
	MP3EncoderLAME(){
		LOGD(__FUNCTION__);
		enc = new liblame::MP3EncoderLAME();
	}

	MP3EncoderLAME(Stream &out_stream){
		LOGD(__FUNCTION__);
		enc = new liblame::MP3EncoderLAME(out_stream);
	}

	~MP3EncoderLAME(){
		LOGD(__FUNCTION__);
		 delete enc;
	 }

	/// Defines the output stream
	void setOutputStream(Print &out_stream){
		LOGD(__FUNCTION__);
        enc->setOutput(out_stream);
    }

	/// Defines the Audio Info
    void setAudioInfo(AudioBaseInfo from) {
		LOGD(__FUNCTION__);
		liblame::AudioInfo info;
		info.channels = from.channels;
		info.sample_rate = from.sample_rate;
		info.bits_per_sample = from.bits_per_sample;
		enc->setAudioInfo(info);
    }

	/// Defines the Audio Info
    
    void setAudioInfo(AudioInfoLAME from) {
		LOGD(__FUNCTION__);
		liblame::AudioInfo info;
		info.channels = from.channels;
		info.sample_rate = from.sample_rate;
		info.bits_per_sample = from.bits_per_sample;
        info.quality = from.quality;
		enc->setAudioInfo(info);
    }

	// starts the processing
	void begin() {
		enc->begin();
	}

	/**
	 * @brief Opens the encoder  
	 * 
	 * @param info 
	 * @return int 
	 */
	void begin(AudioInfoLAME info) {
		LOGD(__FUNCTION__);
		setAudioInfo(info);
        enc->begin();
	}

	void begin(int input_channels, int input_sample_rate, int input_bits_per_sample) {
		LOGD(__FUNCTION__);
        enc->begin(input_channels, input_sample_rate, input_bits_per_sample);
    }

    liblame::AudioInfo audioInfoEx(){
        return enc->audioInfo();
	}

    AudioBaseInfo audioInfo(){
        liblame::AudioInfo from = enc->audioInfo();
		AudioBaseInfo info;
		info.channels = from.channels;
		info.sample_rate = from.sample_rate;
		info.bits_per_sample = from.bits_per_sample;
		return info;
	}
	
	// convert PCM data to convert into MP3
	size_t write(const void *in_ptr, size_t in_size){
		LOGD("write %d bytes", (int) in_size);
		return enc->write((uint8_t*)in_ptr, in_size);
	}

	// release resources
	void end(){
		LOGD(__FUNCTION__);
		enc->end();
	}

	liblame::MP3EncoderLAME *driver() {
		return enc;
	}

	const char *mime() {
		return "audio/mp3";
	}

	virtual operator boolean() {
		return (bool)(*enc);
	}

protected:
	liblame::MP3EncoderLAME *enc=nullptr;

};

}
