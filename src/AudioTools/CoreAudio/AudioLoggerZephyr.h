#pragma once

#if USE_AUDIO_LOGGING
#include <zephyr/logging/log.h>

#ifndef LOG_METHOD
#  define LOG_METHOD __PRETTY_FUNCTION__
#endif

#define TAG_AUDIO "audio-tools"

#define LOGD(...) LOG_DBG(__VA_ARGS__);
#define LOGI(...) LOG_INF(__VA_ARGS__);
#define LOGW(...) LOG_WRN(__VA_ARGS__);
#define LOGE(...) LOG_ERR(__VA_ARGS__);

#define TRACED()  LOG_DBG("%s", LOG_METHOD);
#define TRACEI()  LOG_INF("%s", LOG_METHOD);
#define TRACEW()  LOG_WRN("%s", LOG_METHOD);
#define TRACEE()  LOG_ERR("%s", LOG_METHOD);

#else

// Switch off logging
#define LOGD(...) 
#define LOGI(...) 
#define LOGW(...) 
#define LOGE(...) 
#define TRACED()
#define TRACEI()
#define TRACEW()
#define TRACEE()

#endif
