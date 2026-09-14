#pragma once

#include "AudioToolsConfig.h"
#if defined(ESP32)
#  include "Camera/CameraFrameSourceESP32.h"
#elif defined(USE_OPENCV)
#  include "Camera/CameraFrameSourceOpenCV.h"
#endif