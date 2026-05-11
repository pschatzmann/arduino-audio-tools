#pragma once
#include <stdint.h>

namespace audio_tools {
/**
 * @brief SPI command ids used by SPIAudioMaster
 * @ingroup spi-audio
 *
 * Frame format (master -> slave):
 * - cmd: uint8_t
 * - payload_len: uint16_t (little endian)
 * - payload bytes
 *
 * Response format (slave -> master):
 * - status: uint8_t (0 = ok)
 * - response_len: uint16_t (little endian)
 * - response bytes
 */
enum class SPIAudioCommand : uint8_t {
  SetMime = 1,
  SetAudioInfo = 2,
  WriteData = 5,
  GetAvailableBufferSize = 6,
  GetFilledBufferSize = 7,
  Clear = 8,
};

}  // namespace audio_tools
