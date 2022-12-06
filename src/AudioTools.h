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
#include "AudioTools/SynchronizedBuffers.h"
#include "AudioTools/Converter.h"
#include "AudioFilter/Filter.h"
#include "AudioFilter/Equilizer.h"
#include "AudioTools/MusicalNotes.h"
#include "AudioI2S/I2SStream.h"
#include "AudioPWM/AudioPWM.h"
#include "AudioAnalog/AnalogAudio.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioStreams.h"
#include "AudioTools/AudioStreamsConverter.h"
#include "AudioTools/AudioPrint.h"
#include "AudioTools/Resample.h"
#include "AudioTools/AudioCopy.h"
#include "AudioCodecs/AudioEncoded.h"
#include "AudioCodecs/AudioCodecs.h"
#include "AudioEffects/SoundGenerator.h"
#include "AudioEffects/AudioEffects.h"
#include "AudioEffects/PitchShift.h"
#include "AudioMetaData/MetaData.h"
#include "AudioHttp/AudioHttp.h"
#include "AudioTools/AudioPlayer.h"

/**
 * ------------------------------------------------------------------------- 
 * @brief Optional external libraries
 * 
 */
#if defined(USE_I2S)
#include "AudioTools/AudioSPDIF.h"
#endif

#if defined(USE_PORTAUDIO) 
#include "AudioLibs/PortAudioStream.h"
#endif

#ifdef USE_MOZZI
#include "AudioLibs/AudioMozzi.h"
#endif

#ifdef USE_STK
#include "AudioLibs/AudioSTK.h"
#endif

#ifdef USE_ESP8266_AUDIO
#include "AudioLibs/AudioESP8266.h"
#endif

#ifdef USE_A2DP
#include "AudioLibs/AudioA2DP.h"
#endif

#ifdef USE_EXPERIMENTS
#include "Experiments/Experiments.h"
#endif

/**
 * ------------------------------------------------------------------------- 
 * @brief Set namespace
 * 
 */
#ifndef NO_AUDIOTOOLS_NS
using namespace audio_tools;  
#endif

