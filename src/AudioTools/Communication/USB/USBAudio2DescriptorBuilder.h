#pragma once
#include <cstdint>
#include <cstring>

#include "tusb.h"
#include "USBAudioConfig.h"

namespace audio_tools {

/**
 * @brief USB Audio Class 2.0 descriptor generator.
 *
 * Builds the complete audio-function descriptor block (IAD + AC interface +
 * AS interface(s)) that must be included in the device configuration descriptor.
 *
 * Descriptor layout produced by buildFullDescriptor():
 *   IAD
 *   Standard AC Interface (0 EPs)
 *     CS AC Header  (wTotalLength auto-patched)
 *     Clock Source
 *     [OUT path] Input Terminal (USB Streaming) + Feature Unit + Output Terminal (Speaker)
 *     [IN  path] Input Terminal (Microphone)    + Feature Unit + Output Terminal (USB Streaming)
 *   [OUT AS Interface] alt=0 (zero BW) + alt=1 (CS AS + Format + ISO EP + CS ISO EP)
 *   [IN  AS Interface] alt=0 (zero BW) + alt=1 (CS AS + Format + ISO EP + CS ISO EP)
 */
class USBAudio2DescriptorBuilder {
 public:
  // UAC2 entity IDs — assigned sequentially from 1.
  // Chain 1 is used for the OUT path (or the only path in pure-IN mode).
  // Chain 2 is used for the IN path in RXTX mode.
  static constexpr uint8_t ENTITY_CLOCK = 1;
  static constexpr uint8_t ENTITY_IT1   = 2;  ///< first  Input Terminal
  static constexpr uint8_t ENTITY_FU1   = 3;  ///< first  Feature Unit
  static constexpr uint8_t ENTITY_OT1   = 4;  ///< first  Output Terminal
  static constexpr uint8_t ENTITY_IT2   = 5;  ///< second Input Terminal  (RXTX)
  static constexpr uint8_t ENTITY_FU2   = 6;  ///< second Feature Unit    (RXTX)
  static constexpr uint8_t ENTITY_OT2   = 7;  ///< second Output Terminal (RXTX)

  explicit USBAudio2DescriptorBuilder(USBAudioConfig& cfg) : p_config(&cfg) {}


  // Kept for backward compatibility with existing call sites.
  const uint16_t buildDescriptor(uint8_t /*itf*/, uint8_t /*alt*/, uint8_t* desc) {
    return buildFullDescriptor(desc);
  }

  // Build the complete audio-function descriptor using interface numbers from
  // config.  Returns a pointer to an internal static buffer; *outLen receives
  // the total byte count.
  const uint16_t buildFullDescriptor(uint8_t* desc) {
    return buildFullDescriptor(p_config->itf_num_ac, desc);
  }

  // Same but with an explicit first (AC) interface number.
  const uint16_t buildFullDescriptor(uint8_t first_itf, uint8_t* desc) {
    uint8_t* p = desc;

    const uint8_t itf_ac  = first_itf;
    uint8_t       itf_as  = (uint8_t)(first_itf + 1);
    const uint8_t num_as  = (uint8_t)audioFunctionsCount();

    // ── IAD ──────────────────────────────────────────────────────────────────
    p = writeIAD(p, itf_ac, (uint8_t)(1 + num_as));

    // ── Standard AC Interface ─────────────────────────────────────────────
    const uint8_t ac_nEps = p_config->enable_interrupt_ep ? 1 : 0;
    p = writeStdIface(p, itf_ac, 0, ac_nEps, 0x01 /*AUDIO*/, 0x01 /*CONTROL*/, 0x20 /*UAC2*/);

    // ── CS AC entities — patch wTotalLength after ────────────────────────────
    uint8_t* cs_ac_start = p;
    p = writeCsAcHeader(p);
    p = writeClockSource(p, ENTITY_CLOCK);

    if (p_config->enable_ep_out) {
      // OUT path: USB Streaming input → Feature Unit → Speaker output
      p = writeInputTerminal(p, ENTITY_IT1,
                             0x0101 /*USB Streaming*/, ENTITY_CLOCK);
      p = writeFeatureUnit(p, ENTITY_FU1, ENTITY_IT1);
      p = writeOutputTerminal(p, ENTITY_OT1,
                              0x0301 /*Speaker*/, ENTITY_FU1,
                              ENTITY_CLOCK);
    }

    if (p_config->enable_ep_in) {
      // IN path: Microphone input → Feature Unit → USB Streaming output.
      // Pure-IN mode reuses chain 1 IDs; RXTX mode uses chain 2 IDs.
      const bool    rxtx = p_config->enable_ep_out;
      const uint8_t it   = rxtx ? ENTITY_IT2 : ENTITY_IT1;
      const uint8_t fu   = rxtx ? ENTITY_FU2 : ENTITY_FU1;
      const uint8_t ot   = rxtx ? ENTITY_OT2 : ENTITY_OT1;
      p = writeInputTerminal(p, it, 0x0201 /*Microphone*/, ENTITY_CLOCK);
      p = writeFeatureUnit(p, fu, it);
      p = writeOutputTerminal(p, ot, 0x0101 /*USB Streaming*/, fu,
                              ENTITY_CLOCK);
    }

    // Patch wTotalLength (bytes 6-7 of CS AC Header) — covers only the
    // class-specific descriptors above, not the interrupt endpoint below.
    const uint16_t cs_ac_len = (uint16_t)(p - cs_ac_start);
    cs_ac_start[6] = (uint8_t)(cs_ac_len & 0xFF);
    cs_ac_start[7] = (uint8_t)(cs_ac_len >> 8);

    // Interrupt EP must follow the CS AC block (USB spec §9.6.6:
    // endpoint descriptors come after class-specific interface descriptors).
    if (p_config->enable_interrupt_ep) {
      p = writeInterruptEndpoint(p, p_config->ep_int);
    }

    // ── OUT AS Interface ─────────────────────────────────────────────────────
    if (p_config->enable_ep_out) {
      const uint8_t nEps = enableFeedbackEp() ? 2 : 1;
      p = writeStdIface(p, itf_as, 0, 0,    0x01, 0x02, 0x20); // alt=0 zero BW
      p = writeStdIface(p, itf_as, 1, nEps, 0x01, 0x02, 0x20); // alt=1 active
      p = writeCsAsInterface(p, ENTITY_IT1);  // links to USB Streaming IT
      p = writeFormatType(p);
      p = writeIsoEndpoint(p, p_config->ep_out);
      p = writeCsIsoEndpoint(p);
      if (enableFeedbackEp()) {
        p = writeFeedbackEndpoint(p, p_config->ep_fb);
      }
      ++itf_as;
    }

    // ── IN AS Interface ──────────────────────────────────────────────────────
    if (p_config->enable_ep_in) {
      // Links to the Output Terminal that has USB Streaming type
      const uint8_t ot = p_config->enable_ep_out ? ENTITY_OT2
                                                  : ENTITY_OT1;
      p = writeStdIface(p, itf_as, 0, 0, 0x01, 0x02, 0x20); // alt=0 zero BW
      p = writeStdIface(p, itf_as, 1, 1, 0x01, 0x02, 0x20); // alt=1 active
      p = writeCsAsInterface(p, ot);
      p = writeFormatType(p);
      p = writeIsoEndpoint(p, p_config->ep_in);
      p = writeCsIsoEndpoint(p);
    }

    uint16_t outLen = (uint16_t)(p - desc);
    return outLen;
  }

  int audioFunctionsCount() const {
    return (p_config->enable_ep_in ? 1 : 0) + (p_config->enable_ep_out ? 1 : 0);
  }


  // True when the explicit-feedback endpoint should appear in the descriptor.
  // Mirrors isFeedbackEpEnabled() in USBAudioDeviceBase: feedback is only valid
  // for a pure OUT (speaker) path — with an IN endpoint present the host uses
  // the IN stream as implicit feedback, and TX-only mode has no OUT EP at all.
  bool enableFeedbackEp() const {
    return p_config->enable_feedback_ep
        && p_config->enable_ep_out
        && !p_config->enable_ep_in;
  }

  // Isochronous packet size for one 1 ms frame at a given rate.
  uint16_t calcPacketSizeForRate(uint32_t rate) const {
    return (uint16_t)((p_config->bits_per_sample / 8) * p_config->channels *
                      ((rate + 999) / 1000));
  }

  // Isochronous packet size for one 1 ms frame at the configured rate/format.
  uint16_t calcMaxPacketSize() const {
    return (uint16_t)((p_config->bits_per_sample / 8) * p_config->channels *
                      ((p_config->sample_rate + 999) / 1000));
  }

 protected:
  USBAudioConfig* p_config;

  // ── helpers ─────────────────────────────────────────────────────────────────

  uint32_t channelConfig() const {
    // Stereo: FL+FR = bits 0-1; mono: FL = bit 0; >2ch: lower N bits.
    if (p_config->channels == 1) return 0x00000001u;
    if (p_config->channels == 2) return 0x00000003u;
    return (1u << p_config->channels) - 1u;
  }

  // ── descriptor writers ──────────────────────────────────────────────────────

  // Interface Association Descriptor (8 bytes)
  uint8_t* writeIAD(uint8_t* p, uint8_t first_itf, uint8_t count) {
    *p++ = 8;
    *p++ = 0x0B;        // INTERFACE_ASSOCIATION
    *p++ = first_itf;
    *p++ = count;
    *p++ = 0x01;        // bFunctionClass  AUDIO
    *p++ = 0x00;        // bFunctionSubClass
    *p++ = 0x20;        // bFunctionProtocol UAC2
    *p++ = 0x00;        // iFunction
    return p;
  }

  // Standard Interface Descriptor (9 bytes)
  uint8_t* writeStdIface(uint8_t* p, uint8_t itf, uint8_t alt,
                          uint8_t nEps, uint8_t cls, uint8_t sub, uint8_t proto) {
    *p++ = 9;
    *p++ = 0x04;        // INTERFACE
    *p++ = itf;
    *p++ = alt;
    *p++ = nEps;
    *p++ = cls;
    *p++ = sub;
    *p++ = proto;
    *p++ = 0;           // iInterface
    return p;
  }

  // UAC2 CS AC Interface Header (9 bytes).
  // Caller patches wTotalLength at offsets [6] and [7].
  uint8_t* writeCsAcHeader(uint8_t* p) {
    *p++ = 9;
    *p++ = 0x24;        // CS_INTERFACE
    *p++ = 0x01;        // HEADER
    *p++ = 0x00;
    *p++ = 0x02;        // bcdADC 2.00
    *p++ = 0x08;        // bCategory IO_BOX (generic)
    *p++ = 0x00;        // wTotalLength LSB — patched by caller
    *p++ = 0x00;        // wTotalLength MSB — patched by caller
    *p++ = 0x00;        // bmControls
    return p;
  }

  // UAC2 Clock Source Descriptor (8 bytes)
  //
  // bmAttributes D1..D0 — clock type:
  //   0x00 = external, 0x01 = internal fixed, 0x02 = internal variable,
  //   0x03 = internal programmable
  // bmControls — bit-pair encoding:
  //   D1..D0 = Clock Frequency  (01=read-only, 11=host-programmable)
  //   D3..D2 = Clock Validity   (01=read-only)
  //
  // GET_RANGE returns 14 discrete sample rates (8 kHz – 192 kHz).
  // Host can SET_CUR to any of those rates.
  uint8_t* writeClockSource(uint8_t* p, uint8_t clock_id) {
    *p++ = 8;
    *p++ = 0x24;        // CS_INTERFACE
    *p++ = 0x0A;        // CLOCK_SOURCE
    *p++ = clock_id;
    *p++ = 0x03;        // bmAttributes: internal programmable clock
    *p++ = 0x07;        // bmControls: freq host-programmable (11b), validity read-only (01b)
    *p++ = 0x00;        // bAssocTerminal
    *p++ = 0x00;        // iClockSource
    return p;
  }

  // UAC2 Input Terminal Descriptor (17 bytes)
  uint8_t* writeInputTerminal(uint8_t* p, uint8_t term_id,
                               uint16_t term_type, uint8_t clock_id) {
    const uint32_t ch_cfg = channelConfig();
    *p++ = 17;
    *p++ = 0x24;        // CS_INTERFACE
    *p++ = 0x02;        // INPUT_TERMINAL
    *p++ = term_id;
    *p++ = (uint8_t)(term_type & 0xFF);
    *p++ = (uint8_t)(term_type >> 8);
    *p++ = 0x00;        // bAssocTerminal
    *p++ = clock_id;    // bCSourceID
    *p++ = p_config->channels;
    *p++ = (uint8_t)(ch_cfg & 0xFF);
    *p++ = (uint8_t)((ch_cfg >> 8) & 0xFF);
    *p++ = (uint8_t)((ch_cfg >> 16) & 0xFF);
    *p++ = (uint8_t)((ch_cfg >> 24) & 0xFF);
    *p++ = 0;           // iChannelNames
    *p++ = 0x00;
    *p++ = 0x00;        // bmControls
    *p++ = 0;           // iTerminal
    return p;
  }

  // UAC2 Feature Unit Descriptor (6 + (channels+1)*4 bytes)
  //
  // bmaControls[] is a 32-bit bitmap per channel (+ master at index 0).
  // Each control is a 2-bit pair:
  //   00 = not present, 01 = read-only, 11 = host-programmable
  //
  //   D1..D0 = Mute     (FU_CTRL_MUTE   = 0x01)
  //   D3..D2 = Volume   (FU_CTRL_VOLUME = 0x02)
  //
  // 0x0F = Mute host-programmable (11b) | Volume host-programmable (11b)
  //
  // Volume uses int16 in 1/256 dB units (0x8000 = silence, 0 = 0 dB).
  // AudioTools maps this to float 0.0 (silence) – 1.0 (0 dB).
  // GET_RANGE reports -100 dB .. 0 dB in 1 dB steps.
  uint8_t* writeFeatureUnit(uint8_t* p, uint8_t unit_id, uint8_t src_id) {
    const uint8_t len = (uint8_t)(6 + (p_config->channels + 1) * 4);
    *p++ = len;
    *p++ = 0x24;        // CS_INTERFACE
    *p++ = 0x06;        // FEATURE_UNIT
    *p++ = unit_id;
    *p++ = src_id;
    // Master bmaControls[0]: Mute + Volume, host-programmable
    *p++ = 0x0F; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00;
    // Per-channel bmaControls[1..N]: same controls
    for (uint8_t i = 0; i < p_config->channels; i++) {
      *p++ = 0x0F; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00;
    }
    *p++ = 0x00;        // iFeature
    return p;
  }

  // UAC2 Output Terminal Descriptor (12 bytes)
  uint8_t* writeOutputTerminal(uint8_t* p, uint8_t term_id, uint16_t term_type,
                                uint8_t src_id, uint8_t clock_id) {
    *p++ = 12;
    *p++ = 0x24;        // CS_INTERFACE
    *p++ = 0x03;        // OUTPUT_TERMINAL
    *p++ = term_id;
    *p++ = (uint8_t)(term_type & 0xFF);
    *p++ = (uint8_t)(term_type >> 8);
    *p++ = 0x00;        // bAssocTerminal
    *p++ = src_id;      // bSourceID
    *p++ = clock_id;    // bCSourceID
    *p++ = 0x00;
    *p++ = 0x00;        // bmControls
    *p++ = 0;           // iTerminal
    return p;
  }

  // UAC2 CS AS Interface Descriptor (16 bytes)
  uint8_t* writeCsAsInterface(uint8_t* p, uint8_t terminal_link) {
    const uint32_t ch_cfg = channelConfig();
    *p++ = 16;
    *p++ = 0x24;        // CS_INTERFACE
    *p++ = 0x01;        // AS_GENERAL
    *p++ = terminal_link;
    *p++ = 0x00;        // bmControls
    *p++ = 0x01;        // bFormatType TYPE_I
    // bmFormats: PCM = 0x00000001
    *p++ = 0x01; *p++ = 0x00; *p++ = 0x00; *p++ = 0x00;
    *p++ = p_config->channels;
    *p++ = (uint8_t)(ch_cfg & 0xFF);
    *p++ = (uint8_t)((ch_cfg >> 8) & 0xFF);
    *p++ = (uint8_t)((ch_cfg >> 16) & 0xFF);
    *p++ = (uint8_t)((ch_cfg >> 24) & 0xFF);
    *p++ = 0;           // iChannelNames
    return p;
  }

  // UAC2 Type I Format Type Descriptor (6 bytes)
  uint8_t* writeFormatType(uint8_t* p) {
    *p++ = 6;
    *p++ = 0x24;        // CS_INTERFACE
    *p++ = 0x02;        // FORMAT_TYPE
    *p++ = 0x01;        // FORMAT_TYPE_I
    *p++ = (uint8_t)(p_config->bits_per_sample / 8); // bSubslotSize
    *p++ = p_config->bits_per_sample;                // bBitResolution
    return p;
  }

  // Standard Isochronous Endpoint Descriptor (7 bytes)
  //
  // wMaxPacketSize must be large enough for the highest rate the device
  // advertises in the Clock Source GET_RANGE response.  The actual per-frame
  // byte count varies with the current sample rate; the host will never
  // send or expect more than wMaxPacketSize in a single (micro)frame.
  uint8_t* writeIsoEndpoint(uint8_t* p, uint8_t ep_addr) {
    const uint16_t pkt = calcPacketSizeForRate(192000);
    // bmAttributes: Isochronous (01) + sync type (bits[3:2]) + usage=data (00)
    //   bits 3:2: 00=None, 01=Async, 10=Adaptive, 11=Sync
    // IN  endpoints: Asynchronous (0x05) — device drives the clock.
    // OUT endpoints: Asynchronous (0x05) when a feedback EP is present,
    //               Adaptive (0x09) otherwise (device adapts to host rate).
    bool const is_in  = (ep_addr & 0x80u);
    bool const is_out = !is_in;
    uint8_t bmAttr;
    if (is_in)                             bmAttr = 0x05u;  // ISO + Async
    else if (is_out && enableFeedbackEp()) bmAttr = 0x05u;  // ISO + Async
    else                                   bmAttr = 0x09u;  // ISO + Adaptive
    *p++ = 7;
    *p++ = 0x05;        // ENDPOINT
    *p++ = ep_addr;
    *p++ = bmAttr;
    *p++ = (uint8_t)(pkt & 0xFF);
    *p++ = (uint8_t)(pkt >> 8);
    *p++ = 0x01;        // bInterval (1 = every frame for FS; host uses 125 µs for HS)
    return p;
  }

  // Explicit Feedback Endpoint Descriptor (7 bytes, IN, no CS descriptor)
  uint8_t* writeFeedbackEndpoint(uint8_t* p, uint8_t ep_addr) {
    *p++ = 7;
    *p++ = 0x05;        // ENDPOINT
    *p++ = ep_addr;     // must be an IN address (bit 7 set)
    *p++ = 0x11;        // Isochronous, No Sync, Explicit Feedback usage
    *p++ = 0x04;        // wMaxPacketSize LSB (4 bytes)
    *p++ = 0x00;        // wMaxPacketSize MSB
    *p++ = 0x01;        // bInterval
    return p;
  }

  // Audio Control Interrupt IN Endpoint Descriptor (7 bytes)
  //
  // Carries 6-byte UAC2 status/change notifications (bInfo, bAttribute,
  // wValue = CS<<8|CN, wIndex = EntityID<<8|Itf) so the device can push
  // volume, mute, or sample-rate changes to the host.
  uint8_t* writeInterruptEndpoint(uint8_t* p, uint8_t ep_addr) {
    *p++ = 7;
    *p++ = 0x05;        // ENDPOINT
    *p++ = ep_addr;     // IN address (bit 7 set)
    *p++ = 0x03;        // bmAttributes: Interrupt
    *p++ = 0x06;        // wMaxPacketSize LSB (6 bytes for UAC2 notification)
    *p++ = 0x00;        // wMaxPacketSize MSB
    *p++ = 0x10;        // bInterval: 2^(16-1) = 32768 frames ≈ poll only when needed
    return p;
  }

  // UAC2 CS ISO Endpoint Descriptor (8 bytes)
  uint8_t* writeCsIsoEndpoint(uint8_t* p) {
    *p++ = 8;
    *p++ = 0x25;        // CS_ENDPOINT
    *p++ = 0x01;        // EP_GENERAL
    *p++ = 0x00;        // bmAttributes
    *p++ = 0x00;        // bmControls
    *p++ = 0x00;        // bLockDelayUnits (no locking)
    *p++ = 0x00;
    *p++ = 0x00;        // wLockDelay
    return p;
  }
};

}  // namespace audio_tools
