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
 * @defgroup communications Communications
 * @ingroup main
 * @brief Transmit Audio
 * Please note that the standard Arduino WiFiClient and WifiServer (to use TCP/IP), Serial or BluetoothSerial are also supported.  
 */

/**
 * @defgroup fec FEC
 * @ingroup communications
 * @brief Forward Error Correction
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

#if AUDIO_INCLUDE_CORE

#ifdef USE_CONCURRENCY
#  include "AudioTools/AudioLibs/Concurrency.h"
#endif

#include "AudioTools/CoreAudio.h"
#include "AudioTools/AudioCodecs/AudioEncoded.h"
#include "AudioTools/AudioCodecs/AudioCodecs.h"

/**
 * ------------------------------------------------------------------------- 
 * @brief Optional external libraries
 * 
 */

#if defined(ARDUINO) && !defined(IS_DESKTOP)
#  include "AudioTools/CoreAudio/AudioEffects/Synthesizer.h"
#endif

#endif // AUDIO_INCLUDE_CORE

