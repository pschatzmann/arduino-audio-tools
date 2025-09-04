#pragma once

#include "AudioTools/AudioCodecs/AudioCodecsBase.h"
#include "AudioTools/AudioCodecs/VorbisDecoder.h"
#include "AudioTools/AudioCodecs/ContainerOgg.h"


namespace audio_tools {

/**
 * @brief Ogg Vorbis Decoder
 *
 * This class wraps VorbisDecoder in an Ogg container decoder, allowing
 * decoding of Ogg Vorbis streams with automatic packet extraction.
 *
 * Usage:
 * 1. Instantiate OggVorbisDecoder.
 * 2. Feed Ogg Vorbis data to the decoder.
 * 3. PCM output is provided via the underlying VorbisDecoder.
 *
 * @author Phil Schatzmann
 * @ingroup codecs
 * @ingroup decoder
 * @copyright GPLv3
 */
class OggVorbisDecoder : public OggContainerDecoder {
 public:
	/**
	 * @brief Constructor for OggVorbisDecoder
	 * Initializes the decoder and sets the underlying VorbisDecoder.
	 */
	OggVorbisDecoder() : OggContainerDecoder() { setDecoder(&vorbis); }

 protected:
	/** @brief Underlying Vorbis decoder */
	VorbisDecoder vorbis;
};

}  // namespace audio_tools
