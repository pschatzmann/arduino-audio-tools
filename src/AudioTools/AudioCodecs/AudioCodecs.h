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
#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools/AudioCodecs/CodecCopy.h"
#include "AudioTools/AudioCodecs/CodecL8.h"
#include "AudioTools/AudioCodecs/CodecFloat.h"
#include "AudioTools/AudioCodecs/CodecBase64.h"
#include "AudioTools/AudioCodecs/CodecMTS.h"
#include "AudioTools/AudioCodecs/CodecADTS.h"
#include "AudioTools/AudioCodecs/CodecNetworkFormat.h"
#include "AudioTools/AudioCodecs/CodecFactory.h"
#include "AudioTools/AudioCodecs/StreamingDecoder.h"
#include "AudioTools/AudioCodecs/MultiDecoder.h"
