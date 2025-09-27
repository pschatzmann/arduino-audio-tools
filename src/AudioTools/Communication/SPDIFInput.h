
/**
 * @file SPDIFInput.h
 * @brief Type alias for S/PDIF input on ESP32 platforms.
 *
 * @defgroup spdifinput S/PDIF Input
 * @ingroup audio_tools
 *
 * @section usage Usage Example
 * @code
 * #include <AudioTools.h>
 * SPDIFInput spdif(21); // Use GPIO 21 for input
 * spdif.begin();
 * int16_t buffer[256];
 * int samples = spdif.readArray(buffer, 256);
 * @endcode
 *
 * This type alias maps to SPDIFInputESP32 for ESP32 platforms.
 */
#pragma once
#if defined(ESP32)
#include "SPDIF/SPDIFInputESP32.h"
/**
 * @brief S/PDIF input type for ESP32. See @ref SPDIFInputESP32 for details.
 */
using SPDIFInput = SPDIFInputESP32;
#else
#error("SPDIFInput is only supported by ESP32")
#endif
