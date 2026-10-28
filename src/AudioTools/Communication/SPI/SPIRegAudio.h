#pragma once
/**
 * @ingroup spi-audio
 * @brief Register based SPI Audio: the slave side requires the TinySPISlave
 * library (https://github.com/pschatzmann/TinySPISlave). The master does not
 * need it: SPIRegAudioSlave is only available if the library is installed.
 * @author Phil Schatzmann
 */
#include "SPIRegAudioMaster.h"
#include "SPIRegAudioRegisters.h"
#if defined(__has_include)
#if __has_include("TinySPISlave.h")
#include "SPIRegAudioSlave.h"
#endif
#else
#include "SPIRegAudioSlave.h"
#endif
