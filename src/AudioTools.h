#pragma once

/**
 * @defgroup main Arduino Audio Tools
 * @brief A powerful audio library (not only) for Arduino
 * @file AudioTools.h
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

/**
 * @defgroup tools Tools
 * @ingroup main
 * @brief Div Tools
 */

/**
 * @defgroup io IO
 * @ingroup main
 * @brief Input/Output
 */

/**
 * @defgroup transform Converting Streams
 * @ingroup main
 * @brief Stream classes which change the input or output
 */

/**
 * @defgroup ml Machine Learning
 * @ingroup main
 * @brief Artificial Intelligence
 */

/**
 * @defgroup platform Platform
 * @ingroup main
 * @brief Platform specific implementations.
 * Do not use any of theses classes directly and use the related platform independent
 * typedef instead: 
 * - TimerAlarmRepeating
 * - I2SStrem
 * - PWMStream
 */

/**
 * @defgroup basic Basic
 * @ingroup main
 * @brief Basic Concepts
 */
#include "AudioConfig.h"
#include "AudioTimer/AudioTimer.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/SynchronizedBuffers.h"
#include "AudioTools/BaseConverter.h"
#include "AudioFilter/Filter.h"
#include "AudioFilter/Equilizer.h"
#include "AudioFilter/MedianFilter.h"
#include "AudioTools/MusicalNotes.h"
#include "AudioI2S/I2SStream.h"
#include "AudioPWM/AudioPWM.h"
#include "AudioAnalog/AnalogAudio.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioStreamsConverter.h"
#include "AudioTools/AudioOutput.h"
#include "AudioTools/VolumeStream.h"
#include "AudioTools/AudioIO.h"
#include "AudioTools/ResampleStream.h"
#include "AudioTools/StreamCopy.h"
#include "AudioCodecs/AudioEncoded.h"
#include "AudioCodecs/AudioCodecs.h"
#include "AudioEffects/SoundGenerator.h"
#include "AudioEffects/AudioEffects.h"
#include "AudioEffects/PitchShift.h"
#include "AudioMetaData/MetaData.h"
#include "AudioHttp/AudioHttp.h"
#include "AudioTools/Fade.h"
#include "AudioTools/AudioPlayer.h"

/**
 * ------------------------------------------------------------------------- 
 * @brief Optional external libraries
 * 
 */

#if defined(ARDUINO) && !defined(IS_DESKTOP)
#  include "AudioEffects/Synthesizer.h"
#endif

#if defined(USE_I2S)
#  include "AudioTools/AudioSPDIF.h"
#endif


/**
 * ------------------------------------------------------------------------- 
 * @brief Set namespace
 * 
 */
#ifndef NO_AUDIOTOOLS_NS
using namespace audio_tools;  
#endif

