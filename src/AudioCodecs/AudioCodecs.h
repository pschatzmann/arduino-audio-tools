#pragma once

/** 
 * @defgroup codecs Codecs
 * @ingroup main
 * @brief Audio Coder and Decoder  
**/

/** 
 * @defgroup encoder Encoder
 * @ingroup codecs
 * @brief Audio Encoder 
**/

/** 
 * @defgroup decoder Decoder
 * @ingroup codecs
 * @brief Audio Decoder 
**/

// codecs that do not require any additional library
#include "AudioCodecs/CodecWAV.h"
#include "AudioCodecs/CodecCopy.h"
#include "AudioCodecs/CodecL8.h"
#include "AudioCodecs/CodecFloat.h"
#include "AudioCodecs/CodecBase64.h"
#include "AudioCodecs/DecoderFromStreaming.h"

