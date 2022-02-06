#pragma once

#include "AudioCodecs/CodecWAV.h"
#include "AudioCodecs/CodecNOP.h"
#include "AudioCodecs/CodecRAW.h"

#if defined(USE_HELIX) || defined(USE_DECODERS)
#include "AudioCodecs/CodecHelix.h"
#include "AudioCodecs/CodecAACHelix.h"
#include "AudioCodecs/CodecMP3Helix.h"
#endif

#if defined(USE_FDK) || defined(USE_DECODERS)
#include "AudioCodecs/CodecAACFDK.h"
#endif

#if defined(USE_LAME) || defined(USE_DECODERS)
#include "AudioCodecs/CodecMP3LAME.h"
#endif

#if defined(USE_MAD) || defined(USE_DECODERS)
#include "AudioCodecs/CodecMP3MAD.h"
#endif