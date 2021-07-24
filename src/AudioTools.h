#pragma once

/**
 * @brief Includes all classes
 * @file AudioTools.h
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
#include "AudioConfig.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/Converter.h"
#include "AudioTools/MusicalNotes.h"
#include "AudioTools/SoundGenerator.h"
#include "AudioTools/AudioI2S.h"
#include "AudioTools/AnalogAudio.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/TimerAlarmRepeating.h"
#include "AudioTools/Streams.h"
#include "AudioTools/AudioCopy.h"
#include "AudioTools/AudioPWM.h"
#include "AudioTools/PortAudioStream.h"

#ifdef USE_URL_ARDUINO
// Arduino network support (incl ESP32)
#include "AudioHttp/AudioServer.h"
#include "AudioHttp/URLStreamArduino.h"
#elif defined(ESP32)
// network support with ESP32 API
#include "AudioHttp/AudioServer.h"
#include "AudioHttp/URLStreamESP32.h"
#endif


/**
 * ------------------------------------------------------------------------- 
 * @brief typedefs for DefaultStream
 * 
 */
namespace audio_tools {

#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
typedef PortAudioStream DefaultStream;
#elif defined(ESP32) || defined(ESP8266) || defined(__SAMD21G18A__)
typedef A2DPStream DefaultStream;
#endif

}
