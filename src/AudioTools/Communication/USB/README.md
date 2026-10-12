# USB Audio

This folder provides USB Audio in both USB roles, with the same stream API:

| Class | Role | Use case |
|---|---|---|
| `USBAudioStream` | USB **device** (UAC2) | The microcontroller appears to a PC as a USB speaker and/or microphone. |
| `USBAudioHostStream` | USB **host** (UAC1 and UAC2) | A USB microphone, speaker or headset is plugged into the microcontroller. |

In both roles `readBytes()` receives audio from the USB peer and `write()` sends
audio to it. The direction is selected with `defaultConfig(RX_MODE | TX_MODE | RXTX_MODE)`:

| Mode | `USBAudioStream` (device) | `USBAudioHostStream` (host) |
|---|---|---|
| `RX_MODE` (`readBytes()`) | PC plays to us (we are a speaker) | we record from the attached microphone |
| `TX_MODE` (`write()`) | PC records from us (we are a microphone) | we play to the attached speaker |
| `RXTX_MODE` | both | both (headset) |

## USB Audio Host

```cpp
#include "AudioTools.h"
#include "AudioTools/Communication/USB/USBAudioHostStream.h"

AudioInfo info(48000, 2, 16);
USBAudioHostStream mic;
I2SStream i2s;
StreamCopy copier(i2s, mic);

void setup() {
  mic.addNotifyAudioChange(i2s);  // follow the format of the microphone
  i2s.begin(info);
  auto cfg = mic.defaultConfig(RX_MODE);
  cfg.copyFrom(info);
  mic.begin(cfg);
}

void loop() { copier.copy(); }
```

See the `usb-host-*` sketches in `examples/examples-communication/usb`.

### Format negotiation

The requested `sample_rate`, `channels` and `bits_per_sample` are matched
against the alternate settings of the attached device. If the device cannot
provide them and `host_apply_device_format` is true (the default), the closest
format is used and reported to the listeners registered with
`addNotifyAudioChange()`. In `RXTX_MODE` the microphone format is reported; use
`audioInfoTx()` for the speaker format. Set `host_apply_device_format = false`
to reject a device that does not match.

### Volume, mute and sample rate

`setVolume()` and `setMute()` are sent to the device's Feature Unit, for all
active directions. The 0.0–1.0 volume is mapped linearly onto the dB range the
device reports. `setAudioInfo()` with a new sample rate re-negotiates the
streams (alt setting 0, set the rate, alt setting n).

### Supported platforms

| Platform | Implementation | Board settings |
|---|---|---|
| RP2040 / RP2350 (arduino-pico) | Adafruit TinyUSB host | USB Stack: *Adafruit TinyUSB Host (native)*, or PIO-USB / MAX3421E with `host_rhport = 1` |
| ESP32-S2 / S3 / P4 | ESP-IDF `usb_host` (default) | S3: USB Mode: *Hardware CDC and JTAG*, so the OTG port is free |
| ESP32-S2 / S3 / P4 | Adafruit TinyUSB host | `#define USE_TINYUSB` before the include |
| SAMD, nRF52, classic ESP32, ... | Adafruit TinyUSB + MAX3421E | pass your `Adafruit_USBH_Host` to the constructor (see `usb-host-max3421`) |
| STM32 | Adafruit TinyUSB host | requires an Adafruit TinyUSB build with STM32 host support |
| Renesas (UNO R4), Zephyr | not supported | the cores have no USB host stack |

With TinyUSB on a single-threaded platform, the USB stack runs inside
`readBytes()` and `write()` (`serviceUSB()`). With `USE_FREERTOS`, or on
ESP32, a dedicated task runs it instead.

### Limitations

- One audio device at a time (use `host_vid` / `host_pid` to pick one).
- Only PCM Type I formats with 16, 24 or 32 bit containers.
- The Adafruit TinyUSB Library fetches the configuration descriptor into a
  buffer of `CFG_TUH_ENUMERATION_BUFSIZE` (256) bytes. Devices with a larger
  configuration descriptor (common for headsets) fail enumeration. Increase
  the value in the `tusb_config_<mcu>.h` of that library.
- The RP2040 native host controller has no per-frame isochronous scheduling,
  so the transfers are paced by the frame number. Without a FreeRTOS task,
  call `readBytes()`/`write()` often enough (at least once per millisecond) to
  avoid gaps.
- The TinyUSB backend registers the application class driver
  (`usbh_app_driver_get_cb()`), so it cannot be combined with another custom
  TinyUSB host class driver.
