#pragma once
#include <stdint.h>

namespace audio_tools {

/**
 * @brief Register map shared by SPIRegAudioMaster and SPIRegAudioSlave
 * @ingroup spi-audio
 *
 * Wire protocol (compatible with the TinySPISlave RegisterMap):
 * - command byte: bit7 = read, bits 0..6 = register address
 * - write frame:  `[cmd][bytes...]` (CS low .. CS high)
 * - read request: `[cmd|0x80][n]`   (n = number of bytes wanted)
 * - read reply:   a second frame in which the master clocks n bytes (0x00)
 *   while the slave shifts out the register content. A short pause (CS high)
 *   is required between the two frames.
 *
 * Address 0 is reserved: the dummy bytes clocked by the master during the
 * reply frame are ignored by the slave.
 *
 * All multi byte values are little endian.
 */
enum SPIRegAudioRegister : uint8_t {
  /// Reply clocking / no operation
  SPIREG_NOP = 0x00,
  /// 8 bytes (rw): sample_rate u32, channels u16, bits_per_sample u8, reserved
  SPIREG_AUDIO_INFO = 0x01,
  /// 32 bytes (rw): zero terminated mime type
  SPIREG_MIME = 0x02,
  /// 4 bytes (ro): u32 free space in the slave audio buffer in bytes
  SPIREG_AVAILABLE = 0x03,
  /// stream (wo): the bytes of the frame are appended to the audio buffer
  SPIREG_DATA = 0x10,
};

constexpr uint8_t SPIREG_READ_MASK = 0x80;
constexpr uint8_t SPIREG_ADDR_MASK = 0x7F;
constexpr size_t SPIREG_AUDIO_INFO_SIZE = 8;
constexpr size_t SPIREG_MIME_SIZE = 32;
constexpr size_t SPIREG_AVAILABLE_SIZE = 4;

}  // namespace audio_tools
