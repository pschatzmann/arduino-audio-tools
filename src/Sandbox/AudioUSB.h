#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 48000
#endif

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// RHPort number used for device can be defined by board.mk, default to port 0
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT 0
#endif

// RHPort max operational speed can defined by board.mk
#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED OPT_MODE_DEFAULT_SPEED
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
#ifdef ESP32
#define CFG_TUSB_MCU OPT_MCU_ESP32S2
#else
#error CFG_TUSB_MCU must be defined
#endif

#endif

#if !defined(CFG_TUSB_OS) && !defined(ESP32)
#define CFG_TUSB_OS OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif

// Enable Device stack
#define CFG_TUD_ENABLED 1

// Default is max speed that hardware controller could support with on-chip PHY
#define CFG_TUD_MAX_SPEED BOARD_TUD_MAX_SPEED

// CFG_TUSB_DEBUG is defined by compiler in DEBUG build
// #define CFG_TUSB_DEBUG           0

/* USB DMA on some MCUs can only access a specific SRAM region with restriction
 * on alignment. Tinyusb use follows macros to declare transferring memory so
 * that they can be put into those specific section. e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------
#define _TUSB_CONFIG_H_

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE 64
#endif

//------------- CLASS -------------//
#define CFG_TUD_AUDIO 1
#ifndef ESP32
#define CFG_TUD_CDC 0
#define CFG_TUD_MSC 0
#define CFG_TUD_HID 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0
#endif
//--------------------------------------------------------------------
// AUDIO CLASS DRIVER CONFIGURATION
//--------------------------------------------------------------------

// Have a look into audio_device.h for all configurations

#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN TUD_AUDIO_MIC_ONE_CH_DESC_LEN
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT                                          \
  1 // Number of Standard AS Interface Descriptors (4.9.1) defined per audio
    // function - this is required to be able to remember the current alternate
    // settings of these interfaces - We restrict us here to have a constant
    // number for all audio functions (which means this has to be the maximum
    // number of AS interfaces an audio function has and a second audio function
    // with less AS interfaces just wastes a few bytes)
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ 64 // Size of control request buffer

#define CFG_TUD_AUDIO_ENABLE_EP_IN 1
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX                             \
  2 // Driver gets this info from the descriptors - we define it here to use it
    // to setup the descriptors and to do calculations with it below
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX                                     \
  1 // Driver gets this info from the descriptors - we define it here to use it
    // to setup the descriptors and to do calculations with it below - be aware:
    // for different number of channels you need another descriptor!
#define CFG_TUD_AUDIO_EP_SZ_IN                                                 \
  (48 + 1) *                                                                   \
      CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX *                             \
          CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX // 48 Samples (48 kHz) x 2
                                             // Bytes/Sample x
                                             // CFG_TUD_AUDIO_N_CHANNELS_TX
                                             // Channels - One extra sample is
                                             // needed for asynchronous transfer
                                             // adjustment, see feedback EP
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX                                      \
  CFG_TUD_AUDIO_EP_SZ_IN // Maximum EP IN size for all AS alternate settings
                         // used
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ CFG_TUD_AUDIO_EP_SZ_IN + 1

/* A combination of interfaces must have a unique product id, since PC will save
 * device driver after the first plug. Same VID/PID with different interface e.g
 * MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]     AUDIO | MIDI | HID | MSC | CDC          [LSB]
 */
#ifndef ESP32
#define _PID_MAP(itf, n) ((CFG_TUD_##itf) << (n))
#define USB_PID                                                                \
  (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) |           \
   _PID_MAP(MIDI, 3) | _PID_MAP(AUDIO, 4) | _PID_MAP(VENDOR, 5))
#endif

#include "class/audio/audio_device.h"
#include "tusb.h"

namespace audio_tools {

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,

    // Use Interface Association Descriptor (IAD) for Audio
    // As required by USB Specs IAD's subclass must be common class (2) and
    // protocol must be IAD (1)
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = 0xCafe,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
enum { ITF_NUM_AUDIO_CONTROL = 0, ITF_NUM_AUDIO_STREAMING, ITF_NUM_TOTAL };

#define CONFIG_TOTAL_LEN                                                       \
  (TUD_CONFIG_DESC_LEN + CFG_TUD_AUDIO * TUD_AUDIO_MIC_ONE_CH_DESC_LEN)

#if CFG_TUSB_MCU == OPT_MCU_LPC175X_6X ||                                      \
    CFG_TUSB_MCU == OPT_MCU_LPC177X_8X || CFG_TUSB_MCU == OPT_MCU_LPC40XX
// LPC 17xx and 40xx endpoint type (bulk/interrupt/iso) are fixed by its number
// 0 control, 1 In, 2 Bulk, 3 Iso, 4 In etc ...
#define EPNUM_AUDIO 0x03

#elif TU_CHECK_MCU(OPT_MCU_NRF5X)
// nRF5x ISO can only be endpoint 8
#define EPNUM_AUDIO 0x08

#else
#define EPNUM_AUDIO 0x01
#endif

uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute,
    // power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR(
        /*_itfnum*/ ITF_NUM_AUDIO_CONTROL, /*_stridx*/ 0,
        /*_nBytesPerSample*/ CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX,
        /*_nBitsUsedPerSample*/ CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX * 8,
        /*_epin*/ 0x80 | EPNUM_AUDIO, /*_epsize*/ CFG_TUD_AUDIO_EP_SZ_IN)};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index; // for multiple configurations
  return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "PaniRCorp",                // 1: Manufacturer
    "MicNode",                  // 2: Product
    "123456",                   // 3: Serials, should use chip ID
    "UAC2",                     // 4: Audio Interface
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long
// enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;

  uint8_t chr_count;

  if (index == 0) {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  } else {
    // Convert ASCII string into UTF-16

    if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
      return NULL;

    const char *str = string_desc_arr[index];

    // Cap at max char
    chr_count = (uint8_t)strlen(str);
    if (chr_count > 31)
      chr_count = 31;

    for (uint8_t i = 0; i < chr_count; i++) {
      _desc_str[1 + i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
static int channels = 2;
static uint8_t bytesPerSample;
static Stream *p_in = nullptr;
static Print *p_out = nullptr;

// Audio controls
// Current states
bool mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1]; // +1 for master channel 0
uint16_t
    volume[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1]; // +1 for master channel 0
uint32_t sampFreq;
uint8_t clkValid;

// Range states
audio_control_range_2_n_t(
    1) volumeRng[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1]; // Volume range state
audio_control_range_4_n_t(1) sampleFreqRng; // Sample frequency range state

// Audio test data
uint16_t test_buffer_audio[(CFG_TUD_AUDIO_EP_SZ_IN - 2) / 2];
uint16_t startVal = 0;

void led_blinking_task(void);
void audio_task(void);

/*------------- MAIN -------------*/
class AudioUSB {
public:
  AudioUSB(Stream &in) {
    p_in = &in;
    p_out = &in;
  }
  AudioUSB(Print &out) { p_out = &out; }
  void begin(AudioInfo cfg) {
    this->info = cfg;
    channels = cfg.channels;
    // board_init();

    // init device stack on configured roothub port
    tud_init(BOARD_TUD_RHPORT);

    // Init values
    sampFreq = cfg.sample_rate;
    clkValid = 1;

    sampleFreqRng.wNumSubRanges = 1;
    sampleFreqRng.subrange[0].bMin = cfg.sample_rate;
    sampleFreqRng.subrange[0].bMax = cfg.sample_rate;
    sampleFreqRng.subrange[0].bRes = 0;
  }

  void copy() {
    tud_task(); // tinyusb device task
    led_blinking_task();
    audio_task();
  }

protected:
  AudioInfo info;
};

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) { blink_interval_ms = BLINK_MOUNTED; }

// Invoked when device is unmounted
void tud_umount_cb(void) { blink_interval_ms = BLINK_NOT_MOUNTED; }

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from
// bus
void tud_suspend_cb(bool remote_wakeup_en) {
  (void)remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) { blink_interval_ms = BLINK_MOUNTED; }

//--------------------------------------------------------------------+
// AUDIO Task
//--------------------------------------------------------------------+

void audio_task(void) {
  // Yet to be filled - e.g. put meas data into TX FIFOs etc.
  // asm("nop");
}

//--------------------------------------------------------------------+
// Application Callback API Implementations
//--------------------------------------------------------------------+

// Invoked when audio class specific set request received for an EP
bool tud_audio_set_req_ep_cb(uint8_t rhport,
                             tusb_control_request_t const *p_request,
                             uint8_t *pBuff) {
  (void)rhport;
  (void)pBuff;

  // We do not support any set range requests here, only current value
  // requests
  TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

  // Page 91 in UAC2 specification
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  uint8_t ep = TU_U16_LOW(p_request->wIndex);

  (void)channelNum;
  (void)ctrlSel;
  (void)ep;

  return false; // Yet not implemented
}

// Invoked when audio class specific set request received for an interface
bool tud_audio_set_req_itf_cb(uint8_t rhport,
                              tusb_control_request_t const *p_request,
                              uint8_t *pBuff) {
  (void)rhport;
  (void)pBuff;

  // We do not support any set range requests here, only current value
  // requests
  TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

  // Page 91 in UAC2 specification
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  uint8_t itf = TU_U16_LOW(p_request->wIndex);

  (void)channelNum;
  (void)ctrlSel;
  (void)itf;

  return false; // Yet not implemented
}

// Invoked when audio class specific set request received for an entity
bool tud_audio_set_req_entity_cb(uint8_t rhport,
                                 tusb_control_request_t const *p_request,
                                 uint8_t *pBuff) {
  (void)rhport;

  // Page 91 in UAC2 specification
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  uint8_t itf = TU_U16_LOW(p_request->wIndex);
  uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

  (void)itf;

  // We do not support any set range requests here, only current value
  // requests
  TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

  // If request is for our feature unit
  if (entityID == 2) {
    switch (ctrlSel) {
    case AUDIO_FU_CTRL_MUTE:
      // Request uses format layout 1
      TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_1_t));

      mute[channelNum] = ((audio_control_cur_1_t *)pBuff)->bCur;

      TU_LOG2("    Set Mute: %d of channel: %u\r\n", mute[channelNum],
              channelNum);
      return true;

    case AUDIO_FU_CTRL_VOLUME:
      // Request uses format layout 2
      TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_2_t));

      volume[channelNum] = (uint16_t)((audio_control_cur_2_t *)pBuff)->bCur;

      TU_LOG2("    Set Volume: %d dB of channel: %u\r\n", volume[channelNum],
              channelNum);
      return true;

      // Unknown/Unsupported control
    default:
      TU_BREAKPOINT();
      return false;
    }
  }
  return false; // Yet not implemented
}

// Invoked when audio class specific get request received for an EP
bool tud_audio_get_req_ep_cb(uint8_t rhport,
                             tusb_control_request_t const *p_request) {
  (void)rhport;

  // Page 91 in UAC2 specification
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  uint8_t ep = TU_U16_LOW(p_request->wIndex);

  (void)channelNum;
  (void)ctrlSel;
  (void)ep;

  //	return tud_control_xfer(rhport, p_request, &tmp, 1);

  return false; // Yet not implemented
}

// Invoked when audio class specific get request received for an interface
bool tud_audio_get_req_itf_cb(uint8_t rhport,
                              tusb_control_request_t const *p_request) {
  (void)rhport;

  // Page 91 in UAC2 specification
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  uint8_t itf = TU_U16_LOW(p_request->wIndex);

  (void)channelNum;
  (void)ctrlSel;
  (void)itf;

  return false; // Yet not implemented
}

// Invoked when audio class specific get request received for an entity
bool tud_audio_get_req_entity_cb(uint8_t rhport,
                                 tusb_control_request_t const *p_request) {
  (void)rhport;

  // Page 91 in UAC2 specification
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  // uint8_t itf = TU_U16_LOW(p_request->wIndex); 			// Since
  // we have only one audio function implemented, we do not need the itf value
  uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

  // Input terminal (Microphone input)
  if (entityID == 1) {
    switch (ctrlSel) {
    case AUDIO_TE_CTRL_CONNECTOR: {
      // The terminal connector control only has a get request with only the
      // CUR attribute.
      audio_desc_channel_cluster_t ret;

      // Those are dummy values for now
      ret.bNrChannels = channels;
      ret.bmChannelConfig = (audio_channel_config_t)0;
      ret.iChannelNames = 0;

      TU_LOG2("    Get terminal connector\r\n");

      return tud_audio_buffer_and_schedule_control_xfer(
          rhport, p_request, (void *)&ret, sizeof(ret));
    } break;

      // Unknown/Unsupported control selector
    default:
      TU_BREAKPOINT();
      return false;
    }
  }

  // Feature unit
  if (entityID == 2) {
    switch (ctrlSel) {
    case AUDIO_FU_CTRL_MUTE:
      // Audio control mute cur parameter block consists of only one byte - we
      // thus can send it right away There does not exist a range parameter
      // block for mute
      TU_LOG2("    Get Mute of channel: %u\r\n", channelNum);
      return tud_control_xfer(rhport, p_request, &mute[channelNum], 1);

    case AUDIO_FU_CTRL_VOLUME:
      switch (p_request->bRequest) {
      case AUDIO_CS_REQ_CUR:
        TU_LOG2("    Get Volume of channel: %u\r\n", channelNum);
        return tud_control_xfer(rhport, p_request, &volume[channelNum],
                                sizeof(volume[channelNum]));

      case AUDIO_CS_REQ_RANGE:
        TU_LOG2("    Get Volume range of channel: %u\r\n", channelNum);

        // Copy values - only for testing - better is version below
        audio_control_range_2_n_t(1) ret;

        ret.wNumSubRanges = 1;
        ret.subrange[0].bMin = -90; // -90 dB
        ret.subrange[0].bMax = 90;  // +90 dB
        ret.subrange[0].bRes = 1;   // 1 dB steps

        return tud_audio_buffer_and_schedule_control_xfer(
            rhport, p_request, (void *)&ret, sizeof(ret));

        // Unknown/Unsupported control
      default:
        TU_BREAKPOINT();
        return false;
      }
      break;

      // Unknown/Unsupported control
    default:
      TU_BREAKPOINT();
      return false;
    }
  }

  // Clock Source unit
  if (entityID == 4) {
    switch (ctrlSel) {
    case AUDIO_CS_CTRL_SAM_FREQ:
      // channelNum is always zero in this case
      switch (p_request->bRequest) {
      case AUDIO_CS_REQ_CUR:
        TU_LOG2("    Get Sample Freq.\r\n");
        return tud_control_xfer(rhport, p_request, &sampFreq, sizeof(sampFreq));

      case AUDIO_CS_REQ_RANGE:
        TU_LOG2("    Get Sample Freq. range\r\n");
        return tud_control_xfer(rhport, p_request, &sampleFreqRng,
                                sizeof(sampleFreqRng));

        // Unknown/Unsupported control
      default:
        TU_BREAKPOINT();
        return false;
      }
      break;

    case AUDIO_CS_CTRL_CLK_VALID:
      // Only cur attribute exists for this request
      TU_LOG2("    Get Sample Freq. valid\r\n");
      return tud_control_xfer(rhport, p_request, &clkValid, sizeof(clkValid));

    // Unknown/Unsupported control
    default:
      TU_BREAKPOINT();
      return false;
    }
  }

  TU_LOG2("  Unsupported entity: %d\r\n", entityID);
  return false; // Yet not implemented
}

bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in,
                                   uint8_t cur_alt_setting) {
  (void)rhport;
  (void)itf;
  (void)ep_in;
  (void)cur_alt_setting;

  tud_audio_write((uint8_t *)test_buffer_audio, CFG_TUD_AUDIO_EP_SZ_IN - 2);

  return true;
}

bool tud_audio_tx_done_post_load_cb(uint8_t rhport, uint16_t n_bytes_copied,
                                    uint8_t itf, uint8_t ep_in,
                                    uint8_t cur_alt_setting) {
  (void)rhport;
  (void)n_bytes_copied;
  (void)itf;
  (void)ep_in;
  (void)cur_alt_setting;

  //   for (size_t cnt = 0; cnt < (CFG_TUD_AUDIO_EP_SZ_IN - 2) / 2; cnt++) {
  //     test_buffer_audio[cnt] = startVal++;
  //   }
  if (p_in != nullptr) {
    p_in->readBytes((uint8_t *)test_buffer_audio, CFG_TUD_AUDIO_EP_SZ_IN - 2);
  } else {
    memset(test_buffer_audio, 0, CFG_TUD_AUDIO_EP_SZ_IN - 2);
  }

  return true;
}

bool tud_audio_rx_done_pre_read_cb(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id, uint8_t ep_out, uint8_t cur_alt_setting)
{
    (void)rhport;
    (void)func_id;
    (void)ep_out;
    (void)cur_alt_setting;

    //free-running, adjust myself to match pace
    bytes_read = tud_audio_read(test_buffer_audio, CFG_TUD_AUDIO_EP_SZ_IN - 2);     //read from USB, write to buffer
    if (p_print!=nullptr){
      p_print->write(usb_spk_buf, bytes_read);
    }

    return true;
}


bool tud_audio_set_itf_close_EP_cb(uint8_t rhport,
                                   tusb_control_request_t const *p_request) {
  (void)rhport;
  (void)p_request;
  startVal = 0;

  return true;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void) {
#ifdef PIN_LED
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if (millis() - start_ms < blink_interval_ms)
    return; // not enough time
  start_ms += blink_interval_ms;

  digitalWrite(PIN_LED, led_state);
  led_state = 1 - led_state; // toggle
#endif
}

} // namespace audio_tools