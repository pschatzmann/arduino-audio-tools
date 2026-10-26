#pragma once
#include <stdint.h>

#include <cstring>
#include <vector>

#include "AudioTools/CoreAudio/AudioTypes.h"

namespace audio_tools {

// ── Platform-specific default USB endpoint numbers ──────────────────────────
// Some MCUs' USB peripherals restrict isochronous transfers to one hardwired
// endpoint number instead of allowing the stack to allocate any free one.
//
// nRF52833/nRF52840 caveats:
//  - The USBD peripheral has exactly one isochronous endpoint pair, hardwired
//    to endpoint number 8 (tinyusb dcd_nrf5x.c: EP_ISO_NUM == 8). Opening an
//    isochronous endpoint at any other number hits
//    TU_ASSERT(epnum == EP_ISO_NUM) and the device stops responding -- this
//    is why ep_in/ep_out/ep_fb are pinned here instead of using
//    TinyUSBDevice.allocEndpoint() like on RP2040/SAMD/STM32.
//  - ep_in and ep_fb both resolve to 0x88 (the same physical IN endpoint),
//    which is safe because they're mutually exclusive: enableFeedbackEp()
//    refuses an ep_fb equal to ep_in (see USBAudio2DescriptorBuilder), so
//    enable_feedback_ep_rxtx has no effect here, matching the hardware's
//    single ISO IN + single ISO OUT pair exactly.
//  - ep_int (the AC status/change interrupt endpoint) is NOT restricted --
//    endpoints 1-7 (control/bulk/interrupt) are freely assignable on this
//    chip -- so it keeps the generic -1 default below and is still allocated
//    dynamically even on nRF52.
//  - Because the fixed 0x88/0x08 numbers bypass allocEndpoint() entirely,
//    TinyUSBDevice's internal endpoint counters don't know endpoint 8 is
//    taken. Audio + a single CDC Serial (the common composite case) is safe
//    since CDC only needs a handful of low-numbered endpoints, but stacking
//    many other USB classes alongside Audio could in theory let one of them
//    also land on endpoint 8 and collide -- an edge case, not a concern for
//    a simple Audio+CDC composite device.
#if defined(NRF52840_XXAA) || defined(NRF52833_XXAA)
#ifndef USB_AUDIO_EP_IN
#define USB_AUDIO_EP_IN 0x88
#endif
#ifndef USB_AUDIO_EP_OUT
#define USB_AUDIO_EP_OUT 0x08
#endif
#ifndef USB_AUDIO_EP_FB
#define USB_AUDIO_EP_FB 0x88
#endif
#endif

// USBAudioDeviceESP32 (arduino-esp32) previously pinned 0x83/0x03/0x84/0x85
// here under the assumption that CDC uses 0x81/0x82/0x02. That assumption is
// wrong: arduino-esp32's own USBCDC::load_cdc_descriptor() (USBCDC.cpp)
// hardcodes notification=0x85, OUT=0x03, IN=0x84 -- i.e. exactly the "safe"
// defaults this block used to define -- so a composite CDC + Audio device
// silently collided on three endpoints (confirmed independently: Adafruit's
// own TinyUSBDevice::allocEndpoint() carries a matching ESP32 workaround
// that explicitly steers around 0x03/0x84/0x85 for the same reason). Rather
// than guess another set of "safe" fixed numbers, ESP32 now falls through to
// the generic -1 ("allocate dynamically") default below: USBAudioDeviceESP32
// resolves it via arduino-esp32's own tinyusb_get_free_in_endpoint() /
// tinyusb_get_free_out_endpoint(), which already know about CDC's reserved
// endpoints. See https://github.com/pschatzmann/arduino-audio-tools/issues/2396.

// Arduino UNO R4 / Nano R4 (Renesas RA4M1, arduino:renesas_uno core) caveats:
//  - This core's USB.cpp builds one fixed composite descriptor (CDC + HID +
//    MSD) at runtime and has no dynamic endpoint allocator API (no
//    TinyUSBDevice.allocEndpoint(), no tinyusb_get_free_in_endpoint()), so
//    endpoint numbers must be fixed constants here, same as nRF52 above.
//  - Values must avoid the ones the core's own USB.cpp hardcodes for its
//    built-in interfaces: CDC uses 0x81 (notification), 0x02/0x82 (data);
//    HID uses 0x83; MSD uses 0x04/0x84 (see __USBGetCustomInterfaceDescriptor
//    in USB.h for the authoritative list). EP5-EP7 are free on every
//    variant since none of the built-ins use them.
//  - The RA4M1's RUSB2 USB peripheral (dcd_rusb2.c) only has 2 isochronous-
//    capable pipes, so at most 2 ISO endpoints can be open at once. This
//    matches USBAudioConfig's default that feedback (enable_feedback_ep)
//    is only active in RX-only mode (no IN endpoint) -- TX-only,
//    RX-only-with-feedback, and RXTX-without-feedback each need exactly 2
//    or fewer ISO endpoints. Don't set enable_feedback_ep_rxtx here: RXTX
//    with feedback needs 3.
#if defined(ARDUINO_ARCH_RENESAS)
#ifndef USB_AUDIO_EP_IN
#define USB_AUDIO_EP_IN 0x85
#endif
#ifndef USB_AUDIO_EP_OUT
#define USB_AUDIO_EP_OUT 0x05
#endif
#ifndef USB_AUDIO_EP_FB
#define USB_AUDIO_EP_FB 0x86
#endif
#ifndef USB_AUDIO_EP_INT
#define USB_AUDIO_EP_INT 0x87
#endif
#endif

// Everywhere else (generic TinyUSB: RP2040, SAMD, STM32, and now ESP32) -1
// tells the platform subclass to allocate the endpoint dynamically instead
// of using a fixed number. Define any of these yourself (e.g. via a build
// flag) to support additional boards/cores, or to pin a fixed address,
// without touching the struct or driver.
#ifndef USB_AUDIO_EP_IN
#define USB_AUDIO_EP_IN -1
#endif
#ifndef USB_AUDIO_EP_OUT
#define USB_AUDIO_EP_OUT -1
#endif
#ifndef USB_AUDIO_EP_FB
#define USB_AUDIO_EP_FB -1
#endif
#ifndef USB_AUDIO_EP_INT
#define USB_AUDIO_EP_INT -1
#endif

/**
 * @brief Configuration for USB Audio (inherits sample_rate / channels /
 *        bits_per_sample from AudioInfo).
 */
struct USBAudioConfig : public AudioInfo {

  // ── Direction ─────────────────────────────────────────────────────────────
  bool enable_ep_in = true;   ///< device → host  (capture / microphone)
  bool enable_ep_out = true;  ///< host   → device (playback / speaker)

  // ── USB endpoint addresses ────────────────────────────────────────────────
  /// Addresses must not conflict with other USB interfaces. On most
  /// platforms the -1 default (see USB_AUDIO_EP_* at the top of this file)
  /// lets the platform subclass allocate a free endpoint dynamically, which
  /// avoids conflicts automatically -- including with CDC.
  /// Signed so a negative value (the -1 default on platforms without a
  /// fixed hardware endpoint) can be told apart from a real address: the
  /// platform subclass treats < 0 as "allocate dynamically" and any value
  /// you set >= 0 as a pinned address.
  int16_t ep_in = USB_AUDIO_EP_IN;    ///< ISO IN  (device → host, capture/microphone)
  int16_t ep_out = USB_AUDIO_EP_OUT;  ///< ISO OUT (host → device, playback/speaker)
  int16_t ep_fb = USB_AUDIO_EP_FB;    ///< ISO IN  (explicit feedback, see enable_feedback_ep)
  int16_t ep_int = USB_AUDIO_EP_INT;  ///< INT IN  (AC status/change notifications)

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

  /// Delays readBytes() from returning any data until bufferRx() has filled
  /// past this percentage; purely a startup prime -- once reached, readBytes()
  /// is never gated again for the rest of the session (a later underrun does
  /// not re-arm it). Set to 0 to disable and return data as soon as any is
  /// available. Values above 100 are clamped.
  uint8_t rx_start_fill_percent = 20;

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
  /// By default this only applies in RX-only mode; see
  /// enable_feedback_ep_rxtx for full duplex.
  bool enable_feedback_ep = true;

  /// Also use the feedback endpoint in RXTX (full duplex) mode.  Without it,
  /// an RXTX OUT endpoint is declared adaptive: the host sends at its own
  /// clock, ignores the device's, and setFeedbackPercent() has no effect, so
  /// the application must absorb the clock difference itself (eg: by
  /// resampling).  With it, the OUT endpoint is asynchronous with explicit
  /// feedback, exactly as in RX-only mode.  Needs three isochronous
  /// endpoints (OUT, IN, feedback IN), which nRF52 and Renesas RA4M1 don't
  /// have.  Combine with enable_ep_in_rate_tracking so the IN direction
  /// also follows the device's clock.
  bool enable_feedback_ep_rxtx = false;

  /// When false (default): the clock source is fixed at sample_rate.
  /// The descriptor reports an internal fixed clock, and GET_RANGE returns
  /// only the configured rate.  This avoids complex host negotiation.
  /// When true: the clock source is programmable and GET_RANGE returns the
  /// discrete rates listed in supported_sample_rates below.  The host can
  /// change the rate via SET_CUR, and the descriptor wMaxPacketSize covers
  /// the highest rate in that list.
  bool enable_multi_sample_rate = false;

  /// Discrete sample rates advertised in the Clock Source GET_RANGE response
  /// when enable_multi_sample_rate is true (ignored otherwise). Isochronous
  /// packet/buffer sizing for multi-rate mode is based on the LAST entry, so
  /// keep this sorted ascending -- the host will never actually select a
  /// rate the GET_RANGE response didn't offer, so sizing for anything past
  /// the last entry would just waste buffer/FIFO space (and on MCUs with
  /// small USB FIFO RAM, such as ESP32-S2/S3's DWC2 peripheral, packets
  /// sized for an unreachably high rate can exceed the endpoint's available
  /// FIFO capacity entirely, silently breaking isochronous reception even
  /// though endpoint allocation and transfer arming both report success).
  /// Override before begin() to advertise a different set, e.g. just
  /// {44100, 48000}.
  std::vector<uint32_t> supported_sample_rates = {8000,  11025, 16000, 22050,
                                                   24000, 32000, 44100, 48000};

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

  /// IN endpoint (asynchronous): send one sample more or less per packet as
  /// needed to keep bufferTx() about half full, so the host receives audio
  /// at the rate the application writes it (ie: the device's clock), not at
  /// exactly the nominal rate of the host's clock.  Without it, packets are
  /// always nominal size, so any difference between the two clocks slowly
  /// underflows (zero-padded packets) or overflows bufferTx().  Needs
  /// enable_ep_in_flow_control.  Leave it off if the application writes
  /// "just in time" (eg: from the TX-done callback) rather than at its own
  /// clock, since then bufferTx()'s level says nothing about that clock.
  bool enable_ep_in_rate_tracking = false;

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

  // ── Host-specific (USBAudioHostStream only, ignored by devices) ───────────
  /// Only accept an attached audio device with this vendor id (0 = any).
  uint16_t host_vid = 0;
  /// Only accept an attached audio device with this product id (0 = any).
  uint16_t host_pid = 0;
  /// TinyUSB root hub port used for host mode (e.g. 1 for Pico-PIO-USB).
  uint8_t host_rhport = 0;
  /// Timeout for class specific control requests in ms.
  uint16_t host_ctrl_timeout_ms = 500;
  /// When true, the format which was negotiated with the attached device is
  /// stored in the configuration and reported to subscribed listeners via
  /// setAudioInfo() (e.g. to update the I2S sample rate). When false, an
  /// attached device which can not provide the requested sample_rate,
  /// channels and bits_per_sample is not used.
  bool host_apply_device_format = true;

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
           enable_feedback_ep_rxtx == other.enable_feedback_ep_rxtx &&
           enable_ep_in_flow_control == other.enable_ep_in_flow_control &&
           enable_ep_in_rate_tracking == other.enable_ep_in_rate_tracking &&

           begin_usb == other.begin_usb && terminal_id == other.terminal_id &&

           host_vid == other.host_vid && host_pid == other.host_pid &&
           host_rhport == other.host_rhport &&
           host_apply_device_format == other.host_apply_device_format;
  }

  bool operator!=(const USBAudioConfig& other) { return !(*this == other); }
};

/// Highest rate multi-rate mode should size isochronous packets/buffers for
/// (see supported_sample_rates above). Falls back to the fixed sample_rate
/// if the list was cleared out, so sizing logic never indexes an empty
/// vector.
inline uint32_t highestSupportedSampleRate(const USBAudioConfig& cfg) {
  return !cfg.enable_multi_sample_rate || cfg.supported_sample_rates.empty() ? (uint32_t)cfg.sample_rate
                                             : cfg.supported_sample_rates.back();
}

}  // namespace audio_tools
