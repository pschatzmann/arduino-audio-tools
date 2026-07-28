#pragma once
#include <cstddef>
#include <cstdint>

namespace audio_tools {

/// USB transfer direction, mirrors TinyUSB's TUSB_DIR_OUT/TUSB_DIR_IN (0/1).
enum class UsbDir : uint8_t { Out = 0, In = 1 };

/// USB endpoint transfer type, mirrors tusb_xfer_type_t.
enum class UsbXferType : uint8_t {
  Control = 0,
  Isochronous = 1,
  Bulk = 2,
  Interrupt = 3
};

/// Device enumerated bus speed, mirrors tusb_speed_t's relevant values.
enum class UsbSpeed : uint8_t { Full, High, Low, Unknown };

/// Result of a completed endpoint transfer, mirrors xfer_result_t. The
/// UAC2 driver never inspects this value today (always cast to void), so
/// only a minimal set of results is modeled.
enum class UsbXferResult : uint8_t { Success, Failed, Stalled };

/**
 * @brief Backend-agnostic mirror of the standard 8-byte USB SETUP packet
 *        (TinyUSB's tusb_control_request_t). Identical wire layout on any
 *        USB stack. The bmRequestType bitfields (USB 2.0 spec Table 9-2)
 *        are exposed as accessor methods instead of a bitfield union, so
 *        call sites read almost the same as before:
 *        `p_request->bmRequestType_bit.type` -> `p_request.type()`.
 */
struct UsbSetupPacket {
  uint8_t bmRequestType = 0;
  uint8_t bRequest = 0;
  uint16_t wValue = 0;
  uint16_t wIndex = 0;
  uint16_t wLength = 0;

  enum class Type : uint8_t { Standard = 0, Class = 1, Vendor = 2 };
  enum class Recipient : uint8_t {
    Device = 0,
    Interface = 1,
    Endpoint = 2,
    Other = 3
  };

  Type type() const { return (Type)((bmRequestType >> 5) & 0x03); }
  UsbDir direction() const { return (UsbDir)((bmRequestType >> 7) & 0x01); }
  Recipient recipient() const { return (Recipient)(bmRequestType & 0x1F); }
};

/// Standard request codes the UAC2 driver dispatches on (subset of
/// tusb_request_code_t — values fixed by the USB 2.0 spec).
enum class UsbStdRequest : uint8_t {
  ClearFeature = 1,
  GetInterface = 10,
  SetInterface = 11,
};

/**
 * @brief Backend-agnostic mirror of tusb_desc_interface_t — only the
 *        fields the UAC2 driver reads (USB 2.0 spec Table 9-12).
 */
struct UsbInterfaceDescriptorView {
  uint8_t bInterfaceNumber = 0;
  uint8_t bAlternateSetting = 0;
  uint8_t bNumEndpoints = 0;
  uint8_t bInterfaceClass = 0;
  uint8_t bInterfaceSubClass = 0;
  uint8_t bInterfaceProtocol = 0;
};

/**
 * @brief Backend-agnostic mirror of tusb_desc_endpoint_t (USB 2.0 spec
 *        Table 9-13). wMaxPacketSize is kept in its *raw* little-endian
 *        form (bits 10..0 = size, bits 12..11 = high-speed additional
 *        transactions per microframe) so it round-trips bit-for-bit back
 *        into a real endpoint descriptor for backends that need one; use
 *        packetSize() for the masked value the UAC2 driver's own sizing
 *        logic uses (this mirrors TinyUSB's tu_edpt_packet_size(), which
 *        also only masks the low 11 bits and does not apply the HS
 *        multiplier).
 */
struct UsbEndpointDescriptorView {
  uint8_t bEndpointAddress = 0;
  UsbXferType xferType = UsbXferType::Control;  // bmAttributes bits[1:0]
  uint8_t sync = 0;   // bmAttributes bits[3:2]: none/async/adaptive/sync
  uint8_t usage = 0;  // bmAttributes bits[5:4]: 0=data,1=feedback,2=implicit
  uint8_t bInterval = 0;
  uint16_t wMaxPacketSize = 0;

  UsbDir direction() const {
    return (bEndpointAddress & 0x80) ? UsbDir::In : UsbDir::Out;
  }
  uint16_t packetSize() const { return wMaxPacketSize & 0x7FF; }
};

// ── Descriptor byte-walking helpers ───────────────────────────────────────
// Pure byte arithmetic — works identically against any USB stack's raw
// descriptor buffer. Replace TinyUSB's tu_desc_next/_type/_subtype and
// tu_unaligned_read16/32.

inline uint8_t const* descNext(uint8_t const* p) { return p + p[0]; }
inline uint8_t descType(uint8_t const* p) { return p[1]; }
inline uint8_t descSubtype(uint8_t const* p) { return p[2]; }

inline uint16_t descU16(uint8_t const* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

inline uint32_t descU32(uint8_t const* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

/// Decodes a raw endpoint descriptor's bytes (USB 2.0 spec Table 9-13):
/// [0]=bLength [1]=bDescriptorType [2]=bEndpointAddress [3]=bmAttributes
/// [4..5]=wMaxPacketSize(LE) [6]=bInterval
inline UsbEndpointDescriptorView decodeEndpoint(uint8_t const* p) {
  UsbEndpointDescriptorView v;
  v.bEndpointAddress = p[2];
  uint8_t attr = p[3];
  v.xferType = (UsbXferType)(attr & 0x03);
  v.sync = (attr >> 2) & 0x03;
  v.usage = (attr >> 4) & 0x03;
  v.wMaxPacketSize = descU16(p + 4);
  v.bInterval = p[6];
  return v;
}

/// Decodes a raw interface descriptor's bytes (USB 2.0 spec Table 9-12):
/// [2]=bInterfaceNumber [3]=bAlternateSetting [4]=bNumEndpoints
/// [5]=bInterfaceClass [6]=bInterfaceSubClass [7]=bInterfaceProtocol
inline UsbInterfaceDescriptorView decodeInterface(uint8_t const* p) {
  UsbInterfaceDescriptorView v;
  v.bInterfaceNumber = p[2];
  v.bAlternateSetting = p[3];
  v.bNumEndpoints = p[4];
  v.bInterfaceClass = p[5];
  v.bInterfaceSubClass = p[6];
  v.bInterfaceProtocol = p[7];
  return v;
}

/**
 * @brief Abstract seam for every raw USB-stack call the UAC2 driver
 *        (USBAudioDeviceBase) needs. Concrete subclasses wrap TinyUSB
 *        (USBAudioBackendTinyUSB, today) or a native peripheral driver
 *        (STM32 HAL / Renesas USBFS — future work).
 *
 *        Only linear-buffer transfers are supported — there is no
 *        FIFO-mode transfer API.
 *
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioBackend {
 public:
  virtual ~USBAudioBackend() = default;

  // ── Endpoint lifecycle ──────────────────────────────────────────────────
  /// Open a non-isochronous endpoint (used for the AC interrupt EP, and for
  /// data endpoints on backends where usesIsoAlloc() is false).
  virtual bool openEndpoint(uint8_t rhport,
                            const UsbEndpointDescriptorView& ep) = 0;
  /// Close a previously opened endpoint. Only called when usesIsoAlloc() is
  /// false (backends using iso-alloc keep the endpoint reserved and just
  /// re-activate it on the next alt-setting).
  virtual bool closeEndpoint(uint8_t rhport, uint8_t ep_addr) = 0;
  /// Clear a halted/stalled condition on an endpoint.
  virtual bool clearStall(uint8_t rhport, uint8_t ep_addr) = 0;

  // ── Isochronous endpoint reservation ────────────────────────────────────
  /// True if this backend must pre-allocate iso endpoint buffer space once
  /// (in audiod_open) and only (re)activate it on each SET_INTERFACE,
  /// rather than open/close per alt-setting (mirrors TinyUSB's
  /// TUP_DCD_EDPT_ISO_ALLOC platform capability). Lets the UAC2 driver pick
  /// one code path at runtime instead of branching on a preprocessor macro.
  virtual bool usesIsoAlloc() const = 0;
  /// Reserve buffer space for an isochronous endpoint. Only called when
  /// usesIsoAlloc() is true. Safe to call once per endpoint lifetime.
  virtual bool isoAllocEndpoint(uint8_t rhport, uint8_t ep_addr,
                                uint16_t max_packet_size) = 0;
  /// Activate a previously iso-allocated endpoint for the given descriptor
  /// (arms it to accept traffic at the given alt setting's packet size).
  virtual bool isoActivateEndpoint(uint8_t rhport,
                                   const UsbEndpointDescriptorView& ep) = 0;
  /// Release the busy/claim flag on an iso endpoint without closing it
  /// (used right after a pre-activation during enumeration so a later
  /// transfer can arm normally).
  virtual bool releaseEndpoint(uint8_t rhport, uint8_t ep_addr) = 0;
  /// True if the endpoint is currently free to accept a new transfer (used
  /// to gate feedback sends from the SOF ISR).
  virtual bool claimEndpoint(uint8_t rhport, uint8_t ep_addr) = 0;

  // ── Linear-buffer data transfer ─────────────────────────────────────────
  /// Arm one linear-buffer IN or OUT transfer. `buffer` must stay valid
  /// until the transfer-complete callback fires. Returns false if the
  /// endpoint could not be armed (already busy, not open, etc).
  virtual bool transfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer,
                        uint16_t length) = 0;

  // ── Control endpoint (EP0) ──────────────────────────────────────────────
  /// Complete a GET/SET control transfer's data stage with `buffer`.
  virtual bool controlTransfer(uint8_t rhport, const UsbSetupPacket& request,
                               uint8_t* buffer, uint16_t length) = 0;
  /// Send a zero-length-packet status acknowledgement.
  virtual bool controlStatus(uint8_t rhport,
                             const UsbSetupPacket& request) = 0;

  // ── Device / SOF state ───────────────────────────────────────────────────
  /// Current negotiated bus speed.
  virtual UsbSpeed speed() const = 0;
  /// Convenience: speed() == UsbSpeed::Full.
  bool isFullSpeed() const { return speed() == UsbSpeed::Full; }
  /// Convenience: speed() == UsbSpeed::High.
  bool isHighSpeed() const { return speed() == UsbSpeed::High; }
  /// True once the device has completed USB enumeration.
  virtual bool mounted() const = 0;
  /// Enable/disable the start-of-frame interrupt for the audio feedback
  /// consumer. Backends without a SOF concept (e.g. one that polls
  /// instead) may implement this as a no-op, but must then drive the
  /// feedback-interval logic themselves — not a concern for the TinyUSB
  /// backend.
  virtual void enableSof(uint8_t rhport, bool enable) = 0;
};

}  // namespace audio_tools
