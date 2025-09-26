#pragma once

/**
 * @defgroup rtsp RTSP 
 * @ingroup communications
 * @brief Real Time Streaming Protocol (RTSP) 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools/CoreAudio/AudioPlayer.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "RTSP/IAudioSource.h"
#include "RTSP/RTSPServer.h"
#include "RTSP/RTSPAudioSource.h"
#include "RTSP/RTSPFormat.h"
#include "RTSP/RTSPOutput.h"
#include "RTSP/RTSPAudioStreamer.h"
#include "RTSP/RTSPClient.h"
#ifdef ESP32
#include "RTSP/RTSPPlatformWiFi.h"
#include "RTSP/RTSPClientWiFi.h"
#endif