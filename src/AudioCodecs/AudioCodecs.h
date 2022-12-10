#pragma once

/** 
 * @defgroup codecs Codecs
 * @ingroup main
 * @brief Audio Coder and Decoder  
**/

#include "AudioCodecs/CodecWAV.h"
#include "AudioCodecs/CodecCopy.h"
#include "AudioCodecs/Codec8Bit.h"
#include "AudioCodecs/CodecFloat.h"

#if defined(USE_HELIX) || defined(USE_DECODERS)
#warning "USE_HELIX is obsolete - replace with include"
#include "AudioCodecs/CodecHelix.h"
#include "AudioCodecs/CodecAACHelix.h"
#include "AudioCodecs/CodecMP3Helix.h"
#endif

#if defined(USE_FDK) || defined(USE_DECODERS)
#warning "USE_FDK is obsolete - replace with include"
#include "AudioCodecs/CodecAACFDK.h"
#endif

#if defined(USE_LAME) || defined(USE_DECODERS)
#warning "USE_LAME is obsolete - replace with include"
#include "AudioCodecs/CodecMP3LAME.h"
#endif

#if defined(USE_MAD) || defined(USE_DECODERS)
#warning "USE_MAD is obsolete - replace with include"
#include "AudioCodecs/CodecMP3MAD.h"
#endif