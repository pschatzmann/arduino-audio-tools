#pragma once
#include <stdint.h>

#include <cstring>

#include "AudioTools/CoreAudio/AudioTypes.h"

namespace audio_tools {

#ifdef STANDALONE_USB
/**
 * @brief Configuration for USB Audio (standalone build without AudioInfo).
 * 
 * @author Phil Schatzmann
 * @ingroup usb
 * @copyright GPLv3
 */
struct USBAudioConfig {
  uint32_t sample_rate = 44100;
  uint8_t channels = 2;
  uint8_t bits_per_sample = 16;
#else
/**
 * @brief Configuration for USB Audio (inherits sample_rate / channels /
 *        bits_per_sample from AudioInfo).
 */
struct USBAudioConfig : public AudioInfo {
#endif

  // ── Direction ─────────────────────────────────────────────────────────────
  bool enable_ep_in = true;   ///< device → host  (capture / microphone)
  bool enable_ep_out = true;  ///< host   → device (playback / speaker)

  // ── USB endpoint addresses ────────────────────────────────────────────────
  /// Addresses must not conflict with other USB interfaces (e.g. CDC uses
  /// 0x81, 0x82, 0x02).  The defaults below are safe for CDC + Audio
  /// composite devices.
  uint8_t ep_in = 0x83;   ///< ISO IN  (device → host, capture/microphone)
  uint8_t ep_out = 0x03;  ///< ISO OUT (host → device, playback/speaker)
  uint8_t ep_fb = 0x84;   ///< ISO IN  (explicit feedback, RX-only mode)
  uint8_t ep_int = 0x85;  ///< INT IN  (AC status/change notifications)

  // ── Interface numbering ───────────────────────────────────────────────────
  /// Interface number of the Audio Control interface.
  /// Increment when other USB functions (CDC, HID …) occupy lower numbers.
  uint8_t itf_num_ac = 0;

  // ── Buffering ─────────────────────────────────────────────────────────────
  /// Depth of the audio FIFO expressed as a number of 1 ms packets.
  /// Larger values reduce the risk of underrun/overrun at the cost of latency.
  /// 32 packets (~32 ms at 44.1 kHz stereo 16-bit, ~6 kB RAM) is the minimum
  /// reliable value for RP2040 at 133 MHz: the I2S DMA write can block for
  /// 2-3 ms, and 16 packets leave no margin for those stalls.
  uint8_t fifo_packets = 32;

  // ── Device identity ───────────────────────────────────────────────────────
  uint16_t vid = 0xCafe;
  uint16_t pid = 0x4002;
  const char* manufacturer = "Audio Tools";
  const char* product = "USB Audio";
  const char* serial = "000001";
  bool self_powered = true;
  uint8_t max_power_ma = 100;

  // ── Advanced ──────────────────────────────────────────────────────────────
  /// Enable isochronous feedback endpoint so the host can adjust its clock.
  bool enable_feedback_ep = true;

  /// When false (default): the clock source is fixed at sample_rate.
  /// The descriptor reports an internal fixed clock, and GET_RANGE returns
  /// only the configured rate.  This avoids complex host negotiation.
  /// When true: the clock source is programmable and GET_RANGE returns
  /// 14 discrete rates (8 kHz – 192 kHz).  The host can change the rate
  /// via SET_CUR, and the descriptor wMaxPacketSize covers 192 kHz.
  bool enable_multi_sample_rate = false;

  /// Enable the AC interrupt IN endpoint for device-initiated volume, mute,
  /// and sample-rate change notifications.  Without it the host must poll
  /// via GET_CUR; the controls might still work, just without push updates.
  bool enable_interrupt_ep = false;

  /// Enable UAC2 IN-endpoint flow control: vary the per-frame isochronous
  /// packet size so non-integer sample-per-frame rates are delivered at the
  /// exact average rate.  Without it the device sends a fixed (rounded-up)
  /// packet every frame, which makes e.g. 44100 Hz run at 45000 Hz effective
  /// (45 samples/ms instead of the required 44.1 average).
  bool enable_ep_in_flow_control = true;

  /// Use a flat contiguous buffer for RX instead of a circular FIFO.
  /// Required when the downstream audio driver uses DMA.
  bool use_linear_buffer_rx = true;
  /// Use a flat contiguous buffer for TX instead of a circular FIFO.
  /// Required when the upstream audio driver uses DMA.
  bool use_linear_buffer_tx = true;
  
#if defined(FREERTOS_H) || defined(INC_FREERTOS_H)
  // ── FreeRTOS USB task (RP2040 + FreeRTOS only) ───────────────────────────
  /// When true (default), begin() launches a dedicated FreeRTOS task that
  /// calls tud_task() continuously.  This keeps the USB IN endpoint re-armed
  /// regardless of what the audio loop is doing, and allows the TX buffer to
  /// use a blocking write (see usb_task_write_wait_ms) instead of busy-spinning.
  /// Set to false if you manage your own USB task.
  bool enable_usb_task = true;
  /// Stack depth for the USB task passed directly to xTaskCreate (words).
  /// On 32-bit MCUs 1 word = 4 bytes, so the default 1024 = 4 KB.
  int usb_task_stack_size = 1024;
  /// FreeRTOS priority for the USB task.  Should be high enough to run at
  /// least once per 1 ms USB frame, but below audio-critical tasks.
  int usb_task_priority = configMAX_PRIORITIES - 1;
  /// TX buffer blocking-write timeout in ms when the dedicated USB task is
  /// active.  0 = non-blocking (safe but busy-spins); 5 = block up to 5 ms
  /// (efficient, relies on USB task to drain the buffer).
  int usb_task_write_wait_ms = 5;

#endif
  // ── Volume Handling ────────────────────────────────────────────────────────
  /// When true we will process the volume and mute settings in the audio
  /// stream. When false we will not process the volume and mute settings but just
  /// provide the data for external processing.
  bool volume_active = false;

  // ── ESP32-specific ────────────────────────────────────────────────────────
  /// When true (default), beginUSB() calls USB.begin() automatically.
  /// Set to false for composite USB (e.g. Audio + CDC): register all
  /// interfaces first, then call USB.begin() yourself.
  bool begin_usb = false;

  // ── Zephyr-specific ───────────────────────────────────────────────────────
  /// Terminal ID reported to Zephyr usbd_uac2 callbacks (ignored on TinyUSB).
  /// TX_MODE: Output Terminal ID (device → host, ISO IN).
  /// RX_MODE: Input Terminal ID  (host → device, ISO OUT).
  /// Must match the UAC2 node topology in the board device tree.
  uint8_t terminal_id = 1;

  bool operator==(const USBAudioConfig& other) {
    return sample_rate == other.sample_rate && channels == other.channels &&
           bits_per_sample == other.bits_per_sample &&

           enable_ep_in == other.enable_ep_in &&
           enable_ep_out == other.enable_ep_out &&

           ep_in == other.ep_in && ep_out == other.ep_out &&
           ep_fb == other.ep_fb &&

           itf_num_ac == other.itf_num_ac &&
           fifo_packets == other.fifo_packets &&

           vid == other.vid && pid == other.pid &&

           std::strcmp(manufacturer ? manufacturer : "",
                       other.manufacturer ? other.manufacturer : "") == 0 &&

           std::strcmp(product ? product : "",
                       other.product ? other.product : "") == 0 &&

           std::strcmp(serial ? serial : "",
                       other.serial ? other.serial : "") == 0 &&

           self_powered == other.self_powered &&
           max_power_ma == other.max_power_ma &&

           enable_feedback_ep == other.enable_feedback_ep &&
           enable_ep_in_flow_control == other.enable_ep_in_flow_control &&
           use_linear_buffer_rx == other.use_linear_buffer_rx &&
           use_linear_buffer_tx == other.use_linear_buffer_tx &&

           begin_usb == other.begin_usb && terminal_id == other.terminal_id;
  }

  bool operator!=(const USBAudioConfig& other) { return !(*this == other); }
};

}  // namespace audio_tools
