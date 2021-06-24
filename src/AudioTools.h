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
#include "AudioHttp/AudioServer.h"

#ifdef USE_URL_ARDUINO
#include "AudioHttp/URLStreamArduino.h"
#elif defined(ESP32)
#include "AudioHttp/URLStreamESP32.h"
#endif
