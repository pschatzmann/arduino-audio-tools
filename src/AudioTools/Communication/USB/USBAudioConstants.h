#pragma once
#include <stdint.h>

namespace audio_tools {

// ── UAC2 / USB 2.0 spec constants ───────────────────────────────────────────
// These are fixed byte values defined by the USB 2.0 and UAC1/UAC2 specs, not
// TinyUSB API surface (TinyUSB's own headers just give them names). Owned
// here directly so USBAudioDeviceBase and USBAudioHostBase have zero
// dependency on any particular USB stack's headers.

// Descriptor types (USB 2.0 spec Table 9-5).
static constexpr uint8_t kUsbDescTypeConfiguration = 0x02;
static constexpr uint8_t kUsbDescTypeInterface = 0x04;
static constexpr uint8_t kUsbDescTypeEndpoint = 0x05;
static constexpr uint8_t kUsbDescTypeIad = 0x0B;
static constexpr uint8_t kUsbDescTypeCsInterface = 0x24;
static constexpr uint8_t kUsbDescTypeCsEndpoint = 0x25;

// Audio class/subclass/protocol codes (UAC2 spec Appendix A).
static constexpr uint8_t kUsbClassAudio = 0x01;
static constexpr uint8_t kAudioSubclassControl = 0x01;
static constexpr uint8_t kAudioSubclassStreaming = 0x02;
static constexpr uint8_t kAudioIntProtocolCodeV2 = 0x20;

// AC interface descriptor subtypes (UAC1 Table A-5 / UAC2 Table A-9).
static constexpr uint8_t kAudioCsAcHeader = 0x01;
static constexpr uint8_t kAudioCsAcInputTerminal = 0x02;
static constexpr uint8_t kAudioCsAcOutputTerminal = 0x03;
static constexpr uint8_t kAudioCsAcFeatureUnit = 0x06;
static constexpr uint8_t kAudioCsAcClockSource = 0x0A;  // UAC2 only
// AS interface descriptor subtypes (UAC2 Table A-11).
static constexpr uint8_t kAudioCsAsGeneral = 0x01;
static constexpr uint8_t kAudioCsAsFormatType = 0x02;
// Format type code (UAC1/UAC2): Type I PCM.
static constexpr uint8_t kAudioFormatTypeI = 0x01;
// Control request codes (UAC2 Table A-15). UAC1 uses SET_CUR=0x01,
// GET_CUR=0x81, GET_MIN=0x82, GET_MAX=0x83, GET_RES=0x84.
static constexpr uint8_t kAudioCsReqCur = 0x01;
static constexpr uint8_t kAudioCsReqRange = 0x02;
static constexpr uint8_t kAudio1ReqGetCur = 0x81;
static constexpr uint8_t kAudio1ReqGetMin = 0x82;
static constexpr uint8_t kAudio1ReqGetMax = 0x83;
// Clock Source control selectors (UAC2 Table A-18).
static constexpr uint8_t kAudioCsCtrlSamFreq = 0x01;
static constexpr uint8_t kAudioCsCtrlClkValid = 0x02;
// UAC1 endpoint control selector: Sampling Frequency (UAC1 Table A-19).
static constexpr uint8_t kAudio1EpCtrlSamplingFreq = 0x01;
// Feature Unit control selectors (UAC2 Table A-23).
static constexpr uint8_t AUDIO_FU_CTRL_MUTE = 0x01;
static constexpr uint8_t AUDIO_FU_CTRL_VOLUME = 0x02;
// Terminal type: USB Streaming (UAC2 Table 2-1).
static constexpr uint16_t kAudioTermTypeUsbStreaming = 0x0101;
// Interface Association Descriptor length — fixed 8 bytes.
static constexpr uint16_t kAudioDescIadLen = 8;

// Standard request: SET_INTERFACE (USB 2.0 Table 9-4).
static constexpr uint8_t kUsbReqSetInterface = 0x0B;

// USB control-transfer state-machine stages (universal to any USB stack's
// control-transfer handling, not TinyUSB-specific).
static constexpr uint8_t kControlStageSetup = 1;
static constexpr uint8_t kControlStageData = 2;

static inline uint8_t u16Low(uint16_t v) { return (uint8_t)(v & 0xFF); }
static inline uint8_t u16High(uint16_t v) { return (uint8_t)(v >> 8); }

}  // namespace audio_tools
