#pragma once

#include "AudioConfig.h"
#include "esp_log.h"

#if USE_AUDIO_LOGGING

#ifndef LOG_METHOD
#  define LOG_METHOD __PRETTY_FUNCTION__
#endif

#define TAG_AUDIO "audio-tools"

#define LOGD(...) ESP_LOGD(TAG_AUDIO,__VA_ARGS__);
#define LOGI(...) ESP_LOGI(TAG_AUDIO,__VA_ARGS__);
#define LOGW(...) ESP_LOGW(TAG_AUDIO,__VA_ARGS__);
#define LOGE(...) ESP_LOGE(TAG_AUDIO,__VA_ARGS__);

#define TRACED()  ESP_LOGD(TAG_AUDIO, "%s", LOG_METHOD);
#define TRACEI()  ESP_LOGI(TAG_AUDIO, "%s", LOG_METHOD);
#define TRACEW()  ESP_LOGW(TAG_AUDIO, "%s", LOG_METHOD);
#define TRACEE()  ESP_LOGE(TAG_AUDIO, "%s", LOG_METHOD);

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
