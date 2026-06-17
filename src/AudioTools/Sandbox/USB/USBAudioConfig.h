#pragma once
#include "stdint.h"
#include "AudioTools/CoreAudio/AudioTypes.h"

namespace audio_tools {

#ifdef STANDALONE_USB
/**
 * @brief Configuration for USB Audio (standalone build without AudioInfo).
 */
struct USBAudioConfig {
  uint32_t sample_rate     = 44100;
  uint8_t  channels        = 2;
  uint8_t  bits_per_sample = 16;
#else
/**
 * @brief Configuration for USB Audio (inherits sample_rate / channels /
 *        bits_per_sample from AudioInfo).
 */
struct USBAudioConfig : public AudioInfo {
#endif

  // ── Direction ─────────────────────────────────────────────────────────────
  bool enable_ep_in  = true;   ///< device → host  (capture / microphone)
  bool enable_ep_out = true;   ///< host   → device (playback / speaker)

  // ── USB endpoint addresses ────────────────────────────────────────────────
  /// Must match the TinyUSB / hardware endpoint assignment.
  uint8_t ep_in  = 0x81;
  uint8_t ep_out = 0x01;
  uint8_t ep_fb = 0x82;

  // ── Interface numbering ───────────────────────────────────────────────────
  /// Interface number of the Audio Control interface.
  /// Increment when other USB functions (CDC, HID …) occupy lower numbers.
  uint8_t itf_num_ac = 0;

  // ── Buffering ─────────────────────────────────────────────────────────────
  /// Depth of the audio FIFO expressed as a number of 1 ms packets.
  /// Larger values reduce the risk of underrun/overrun at the cost of latency.
  uint8_t fifo_packets = 16;

  // ── Device identity ───────────────────────────────────────────────────────
  uint16_t    vid          = 0xCafe;
  uint16_t    pid          = 0x4002;
  const char* manufacturer = "Audio Tools";
  const char* product      = "USB Audio";
  const char* serial       = "000001";
  bool        self_powered = true;
  uint8_t     max_power_ma = 100;

  // ── Advanced ──────────────────────────────────────────────────────────────
  /// Enable isochronous feedback endpoint so the host can adjust its clock.
  bool enable_feedback_ep = true;

  /// Use a flat contiguous buffer for RX instead of a circular FIFO.
  /// Required when the downstream audio driver uses DMA.
  bool use_linear_buffer_rx = false;
  /// Use a flat contiguous buffer for TX instead of a circular FIFO.
  /// Required when the upstream audio driver uses DMA.
  bool use_linear_buffer_tx = false;

  // ── Zephyr-specific ───────────────────────────────────────────────────────
  /// Terminal ID reported to Zephyr usbd_uac2 callbacks (ignored on TinyUSB).
  /// TX_MODE: Output Terminal ID (device → host, ISO IN).
  /// RX_MODE: Input Terminal ID  (host → device, ISO OUT).
  /// Must match the UAC2 node topology in the board device tree.
  uint8_t terminal_id = 1;
};

}  // namespace audio_tools
