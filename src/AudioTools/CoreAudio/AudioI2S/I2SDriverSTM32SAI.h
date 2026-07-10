#pragma once

// SAI1 is the CMSIS peripheral-base macro that only exists when this MCU
// actually has a SAI peripheral - gating on it (in addition to the
// stm32-sai library being installed, checked by I2SStream.h before
// including this file) means we never attempt to compile this against a
// chip like the STM32F411 that has no SAI hardware at all, which would
// otherwise fail deep inside stm32-sai's own HAL-based driver code instead
// of just cleanly not being included.
#if defined(STM32) && defined(SAI1)
#include "AudioTools/CoreAudio/AudioI2S/I2SConfig.h"
#include "AudioTools/CoreAudio/AudioI2S/I2SDriverBase.h"
#include "STM32AudioSAI.h"

namespace audio_tools {

/**
 * @brief I2S API for the STM32 SAI peripheral, using
 * https://github.com/pschatzmann/stm32-sai (DRAFT). This is an alternative
 * to I2SDriverSTM32, which drives the classic SPI/I2S peripheral via
 * https://github.com/pschatzmann/stm32f411-i2s. Not all STM32 parts have a
 * SAI peripheral, and stm32-sai currently only ships board configs for a
 * subset of them - check that library's STM32DriverAll.h for the boards it
 * supports.
 *
 * On boards where I2SDriverSTM32 could not claim IS_I2S_IMPLEMENTED (no
 * STM_I2S_PINS defined for the board in stm32-i2s), this becomes the
 * default I2SDriver automatically, so plain `I2SStream i2s;` already uses
 * SAI. Since I2SStream is not templated, on boards where classic I2S *is*
 * available you can still opt into SAI explicitly via its I2SDriverBase&
 * constructor:
 * @code
 * I2SDriverSTM32SAI sai_driver;
 * I2SStream i2s(sai_driver);
 * @endcode
 *
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SDriverSTM32SAI : public I2SDriverBase {
 public:
  /// Provides the default configuration
  I2SConfigStd defaultConfig(RxTxMode mode = TX_MODE) {
    I2SConfigStd c(mode);
    return c;
  }

  /// Live sample-rate updates are not supported: I2SStream will end()/begin() instead
  bool setAudioInfo(AudioInfo info) { return false; }

  /// starts the SAI with the default config in TX Mode
  bool begin(RxTxMode mode = TX_MODE) { return begin(defaultConfig(mode)); }

  /// starts the SAI
  bool begin(I2SConfigStd cfg) {
    TRACED();
    this->cfg = cfg;

    if (cfg.channels > 2 || cfg.channels <= 0) {
      LOGE("Channels not supported: %d", cfg.channels);
      return false;
    }

    sai.setMode(toMode(cfg.rx_tx_mode));
    sai.setMaster(cfg.is_master);
    sai.setSampleRate(cfg.sample_rate);
    sai.setBitsPerSample(cfg.bits_per_sample);
    sai.setChannels(cfg.channels);
    sai.setProtocol(STM32AudioSAI::I2S);
    sai.setDataFormat(toDataFormat(cfg.i2s_format));
#if defined(STM32F723xx)
    // The STM32F723E-Discovery's WM8994 is wired to SAI2 using the ST BSP's
    // exact frame layout (SAIx_Out_Init in stm32f723e_discovery_audio.c): a
    // 4-slot, 64-bit TDM frame with all 4 slots active, even though only 2
    // slots (stereo) carry real DAC data - the codec's AIF only recognizes
    // its configured timeslot within that specific frame shape. A plain
    // 2-slot/32-bit frame (this driver's default) does not match what the
    // codec's AIF expects here and produces complete silence despite
    // otherwise-correct codec register state and error-free DMA transfers.
    sai.setSlotCount(4);
    sai.setActiveSlots(SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_1 | SAI_SLOTACTIVE_2 |
                        SAI_SLOTACTIVE_3);
#endif
    setupPins();

    active = sai.begin();
    return active;
  }

  /// stops the SAI
  void end() {
    TRACED();
    sai.end();
    active = false;
  }

  /// number of bytes available for reading
  int available() { return active ? sai.available() : 0; }

  /// number of bytes we can still write w/o blocking
  int availableForWrite() { return active ? sai.availableForWrite() : 0; }

  /// provides the actual configuration
  I2SConfigStd config() { return cfg; }

  /// blocking writes for the data to the SAI interface
  size_t writeBytes(const void *src, size_t size_bytes) {
    if (!active) return 0;
    return sai.write((const uint8_t *)src, size_bytes);
  }

  size_t readBytes(void *dest, size_t size_bytes) {
    if (!active) return 0;
    return sai.readBytes((uint8_t *)dest, size_bytes);
  }

  /// Provides access to the underlying stm32-sai driver, e.g. to call
  /// setIOTimoutMs() or setPin() with settings not covered by I2SConfig.
  STM32AudioSAI &driver() { return sai; }

 protected:
  STM32AudioSAI &sai = SAI;
  I2SConfigStd cfg;
  bool active = false;

  STM32AudioSAI::Mode toMode(RxTxMode mode) {
    switch (mode) {
      case RX_MODE:
        return STM32AudioSAI::Input;
      case TX_MODE:
        return STM32AudioSAI::Output;
      case RXTX_MODE:
      default:
        return STM32AudioSAI::Duplex;
    }
  }

  STM32AudioSAI::DataFormat toDataFormat(I2SFormat fmt) {
    switch (fmt) {
      case I2S_LSB_FORMAT:
      case I2S_RIGHT_JUSTIFIED_FORMAT:
        return STM32AudioSAI::RightJustified;
      case I2S_MSB_FORMAT:
      case I2S_LEFT_JUSTIFIED_FORMAT:
        return STM32AudioSAI::LeftJustified;
      case I2S_STD_FORMAT:
      case I2S_PHILIPS_FORMAT:
      default:
        return STM32AudioSAI::Standard;
    }
  }

  /// cfg pins default to -1 ("use the board's compiled-in stm32-sai default
  /// pins" - see STM32SAIDriverConfig::defaultPins), so only override them
  /// when the sketch actually set one explicitly.
  void setupPins() {
    setPinIfDefined(PinId::SCK, cfg.pin_bck);
    setPinIfDefined(PinId::FS, cfg.pin_ws);
    setPinIfDefined(PinId::MCLK, cfg.pin_mck);
    setPinIfDefined(PinId::SD, cfg.pin_data);
  }

  void setPinIfDefined(PinId id, int arduino_pin) {
    if (arduino_pin == -1) return;
    PinName name = digitalPinToPinName(arduino_pin);
    int8_t port = 'A' + STM_PORT(name);
    int8_t pin = STM_PIN(name);
    sai.setPin(id, port, pin, cfg.pin_alt_function);
  }
};

using SAIDriver = I2SDriverSTM32SAI;

}  // namespace audio_tools

// Only claim the I2SDriver/IS_I2S_IMPLEMENTED slot if the classic I2S
// peripheral (I2SDriverSTM32, gated on STM_I2S_PINS) hasn't already been
// wired up for this board - that keeps existing boards on their current
// driver and only makes SAI the default where classic I2S isn't available.
#ifndef IS_I2S_IMPLEMENTED
#define IS_I2S_IMPLEMENTED
namespace audio_tools {
using I2SDriver = I2SDriverSTM32SAI;
}
#endif

#endif
