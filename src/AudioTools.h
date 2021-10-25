#pragma once

/**
 * @brief Includes all classes
 * @file AudioTools.h
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
#include "AudioConfig.h"
#include "AudioTimer/AudioTimer.h"
#include "AudioTools/AudioTypes.h"
#include "AudioTools/Buffers.h"
#include "AudioTools/Converter.h"
#include "AudioTools/MusicalNotes.h"
#include "AudioTools/SoundGenerator.h"
#include "AudioI2S/I2SStream.h"
#include "AudioPWM/AudioPWM.h"
#include "AudioAnalog/AnalogAudio.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioOutput.h"
#include "AudioTools/AudioCopy.h"
#include "AudioTools/PortAudioStream.h"
#include "AudioTools/MetaDataID3.h"
#include "AudioCodecs/CodecWAV.h"
#include "AudioHttp/AudioHttp.h"
#include "AudioTools/AudioPlayer.h"


/**
 * ------------------------------------------------------------------------- 
 * @brief typedefs for Default
 * 
 */
namespace audio_tools {

#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
typedef PortAudioStream DefaultStream;
#elif defined(ESP32) || defined(ESP8266) || defined(__SAMD21G18A__)
typedef I2SStream DefaultStream;
#endif

}
