# USB Audio Class 2.0 for Arduino

I'm happy to announce that the [Arduino Audio Tools library](https://github.com/pschatzmann/arduino-audio-tools) now includes native USB Audio Class 2.0 (UAC2) support. Your ESP32-S3 or RP2040 board can show up as a standard USB microphone, speaker, or both — no drivers needed on the host side. Linux, macOS, and Windows 10+ recognize it out of the box.

## Why USB Audio?

Most Arduino audio projects use I2S to talk to an external DAC or ADC. That works well, but it requires extra hardware and doesn't help when you want to stream audio directly between a microphone and a computer, or use your board as a USB sound card.

With UAC2, the microcontroller *is* the sound card. Plug it in, and tools like `arecord`, `aplay`, Audacity, or any DAW can send and receive audio without any custom software on the host.

## What's Supported

- **TX mode** (device → host): The board appears as a USB microphone. Feed it audio from any source — I2S, a sine generator, an MP3 decoder — and it streams to the host as a standard capture device.
- **RX mode** (host → device): The board appears as a USB speaker. The host plays audio, and your sketch reads it with `readBytes()` to forward to a DAC, process, or analyze.
- **RXTX mode**: Both directions at once. The board appears as a composite capture + playback device.
- **Volume and mute control**: The host can adjust per-channel volume and mute through the standard mixer interface (e.g. `amixer`, PulseAudio, Windows volume mixer).
- **Sample rate flexibility**: 16-bit, 24-bit, and 32-bit PCM at any standard rate. An optional multi-rate mode advertises 14 discrete rates from 8 kHz to 192 kHz.
- **Composite USB**: Works alongside USB CDC (serial), so you can debug over the same USB cable.
- **Cross-platform**: Tested on Linux (`snd-usb-audio`), and designed to work with macOS CoreAudio and Windows UAC2 drivers.

## Supported Hardware

- **ESP32-S2 / ESP32-S3** — via the native USB OTG peripheral (DWC2 controller)
- **Adafruit TinyUSB stack** all boards supporting the Adafruit TinyUSB stack (e.g. Raspberry Pi Pico)
- **Zephyr-based boards** — via Zephyr's native `usbd_uac2` driver (nRF5340, STM32, etc.)

## Getting Started

### TX Mode — USB Microphone

A minimal example that streams a sine wave to the host:

```cpp
#include "AudioTools.h"
#include "AudioTools/Communication/USB/USBAudioStream.h"

AudioInfo info(44100, 2, 16);
SineGenerator<int16_t> sineWave;
GeneratedSoundStream<int16_t> sound(sineWave);
USBAudioStream out;
StreamCopy copier(out, sound);

void setup() {
  // Required on cores without built-in TinyUSB support (e.g. mbed RP2040)
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  sineWave.begin(info, N_B4);
  out.addNotifyAudioChange(sound);

  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  out.begin(config);

  // Re-enumerate so the host picks up the new audio interface
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}

void loop() {
  copier.copy();
}
```

On the host, the device appears immediately:

```bash
arecord -D hw:Audio -f S16_LE -r 44100 -c 2 -d 5 recording.wav
```

### RX Mode — USB Speaker

The board receives audio from the host:

```cpp
#include "AudioTools.h"
#include "AudioTools/Communication/USB/USBAudioStream.h"

AudioInfo info(44100, 2, 16);
USBAudioStream in;
I2SStream i2s;  // or any output
StreamCopy copier(i2s, in);

void setup() {
  // Required on cores without built-in TinyUSB support (e.g. mbed RP2040)
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  auto config = in.defaultConfig(RX_MODE);
  config.copyFrom(info);
  in.begin(config);

  auto i2s_cfg = i2s.defaultConfig(TX_MODE);
  i2s_cfg.copyFrom(info);
  i2s.begin(i2s_cfg);

  // Re-enumerate so the host picks up the new audio interface
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}

void loop() {
  copier.copy();
}
```

Then on the host:

```bash
aplay -D hw:Audio my_song.wav
```

## Configuration

All USB audio settings live in a single `USBAudioConfig` struct. You get a pre-filled instance from `defaultConfig()` and customize what you need:

```cpp
auto config = usb.defaultConfig(TX_MODE);  // or RX_MODE, RXTX_MODE
config.copyFrom(info);                     // inherit sample_rate, channels, bits_per_sample
```

### Audio Format

| Field | Default | Description |
|-------|---------|-------------|
| `sample_rate` | 44100 | Sample rate in Hz. Inherited from `AudioInfo`. |
| `channels` | 2 | Number of audio channels (1 = mono, 2 = stereo). |
| `bits_per_sample` | 16 | Bit depth per sample. Must be 16, 24, or 32. Maps to S16_LE, S24_3LE, S32_LE on the host. |

### Direction

| Field | Default | Description |
|-------|---------|-------------|
| `enable_ep_in` | true | Enable the IN endpoint (device → host, capture/microphone). |
| `enable_ep_out` | true | Enable the OUT endpoint (host → device, playback/speaker). |

`defaultConfig(TX_MODE)` sets `enable_ep_in=true, enable_ep_out=false`. `defaultConfig(RX_MODE)` does the opposite. `RXTX_MODE` enables both.

### Endpoint Addresses

| Field | Default | Description |
|-------|---------|-------------|
| `ep_in` | 0x83 | ISO IN endpoint address (capture). |
| `ep_out` | 0x03 | ISO OUT endpoint address (playback). |
| `ep_fb` | 0x84 | ISO IN feedback endpoint (RX-only mode). |
| `ep_int` | 0x85 | INT IN endpoint (AC change notifications). |

The defaults are chosen to avoid conflicts with CDC, which uses 0x81, 0x82, and 0x02.

### Buffering

| Field | Default | Description |
|-------|---------|-------------|
| `fifo_packets` | 16 | Number of 1 ms USB packets buffered. Higher values reduce the risk of underrun at the cost of latency. |
| `use_linear_buffer_rx` | true | Use a flat buffer for RX (required for DMA-based downstream drivers). |
| `use_linear_buffer_tx` | true | Use a flat buffer for TX. |

### Device Identity

| Field | Default | Description |
|-------|---------|-------------|
| `vid` | 0xCafe | USB Vendor ID. |
| `pid` | 0x4002 | USB Product ID. |
| `manufacturer` | "Audio Tools" | Manufacturer string shown by the host. |
| `product` | "USB Audio" | Product name shown by the host. |
| `serial` | "000001" | Serial number string. |
| `self_powered` | true | Device is self-powered (not bus-powered). |
| `max_power_ma` | 100 | Maximum current draw in mA. |

### Feature Flags

| Field | Default | Description |
|-------|---------|-------------|
| `enable_feedback_ep` | true | Enable the isochronous feedback endpoint so the host can adjust its clock. Only active in pure RX mode (no IN endpoint). |
| `enable_multi_sample_rate` | false | When true, the clock source is programmable and GET_RANGE advertises 14 discrete rates (8 kHz – 192 kHz). When false, only the configured rate is reported. |
| `enable_interrupt_ep` | false | Enable the AC interrupt endpoint for device-initiated volume, mute, and sample-rate change notifications. |
| `enable_ep_in_flow_control` | true | Vary the per-frame packet size so non-integer rates like 44100 Hz are delivered at the exact average rate. |
| `volume_active` | false | When true, the library applies volume and mute scaling to the audio samples directly. When false, values are available via `volume()` and `isMuted()` for external processing. |

### ESP32-Specific

| Field | Default | Description |
|-------|---------|-------------|
| `begin_usb` | false | When true, `beginUSB()` calls `USB.begin()` automatically. Set to false for composite USB devices where you control the startup order. |

### Example

```cpp
auto config = usb.defaultConfig(TX_MODE);
config.copyFrom(info);
config.product = "My Synth";
config.vid = 0x1234;
config.pid = 0x5678;
config.fifo_packets = 32;         // more buffering
config.volume_active = true;      // let the library handle volume
config.enable_interrupt_ep = true; // push volume/rate changes to host
usb.begin(config);
```

## Volume and Mute

The UAC2 descriptors advertise per-channel volume and mute controls. The host sees them as standard mixer controls:

```bash
# List controls
amixer -c Audio scontrols

# Set volume to 80%
amixer -c Audio sset 'Mic',0 80%

# Mute
amixer -c Audio sset 'Mic',0 mute
```

On the device side, you can react to volume changes with a callback:

```cpp
out.setVolumeCallback([](float vol, uint8_t channel) {
  Serial.printf("Volume ch%d: %.0f%%\n", channel, vol * 100);
});

out.setMuteCallback([](bool muted, uint8_t channel) {
  Serial.printf("Mute ch%d: %s\n", channel, muted ? "on" : "off");
});
```

Or enable automatic volume processing so the library scales the audio samples directly:

```cpp
config.volume_active = true;
```

## Architecture

The implementation is split into layers:

- **`USBAudioConfig`** — all configuration in one struct: sample rate, channels, bit depth, endpoint addresses, buffering, device identity, and feature flags.
- **`USBAudio2DescriptorBuilder`** — generates the complete UAC2 descriptor block at runtime: IAD, Audio Control interface with Clock Source, Input/Output Terminals, and Feature Units, plus Audio Streaming interfaces with Format Type descriptors and isochronous endpoints.
- **`USBAudioDeviceBase`** — the core class (~2200 lines) that implements the TinyUSB class driver interface: descriptor callbacks, control request handling (sample rate, volume, mute), isochronous endpoint management, flow control for accurate sample rates, and the `AudioStream` read/write API.
- **`USBAudioDeviceESP32`** / **`USBAudioDeviceTinyUSB`** / **`USBAudioDeviceZephyr`** — thin platform subclasses that provide the right buffer implementation and USB stack initialization for each target.
- **`USBAudioStream`** — a dispatch header that resolves to the correct platform class automatically.

The key design goal was to make USB audio feel like any other AudioTools stream. `StreamCopy` works. `addNotifyAudioChange` works. Volume and mute integrate with the existing `VolumeSupport` interface. You can chain it with any source or sink in the library.

## Flow Control

One subtle but important feature is TX flow control. At 44100 Hz with 1 ms USB frames, you need to send 44.1 samples per frame — not an integer. Without flow control, you'd send 45 samples every frame, and the effective rate would drift to 45000 Hz.

The flow control implementation uses a fractional accumulator: it alternates between 44-sample and 45-sample packets so the average over time is exactly 44.1 samples per frame. This is enabled by default (`enable_ep_in_flow_control = true`) and is essential for glitch-free audio.

## Buffering

Getting buffer management right on a dual-core chip like the ESP32-S3 was one of the bigger challenges. The USB stack runs on core 0, but the Arduino `loop()` typically runs on core 1. We need lock-free or RTOS-synchronized handoff between the cores.

The ESP32 subclass uses `BufferRTOS` — a FreeRTOS StreamBuffer wrapper that provides safe cross-core data transfer. The TX side uses a short write timeout (5 ms) so the audio copier blocks briefly when the buffer is full, while the USB callback side never blocks. The RX side is fully non-blocking on both ends.

On the RP2040 (single-core), a simple `RingBuffer` is sufficient since there's no cross-core contention.

## Composite USB Devices

You can combine USB Audio with CDC serial for debugging. On ESP32-S3, set `begin_usb = false` in the config, register the CDC interface first, then start audio, and finally call `USB.begin()` yourself:

```cpp
USBCDC MySerial;

void setup() {
  MySerial.begin(115200);

  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.begin_usb = false;  // we'll call USB.begin() manually
  out.begin(config);

  USB.begin();  // starts both CDC and Audio
}
```

The default endpoint addresses (0x83, 0x03, 0x84, 0x85) are chosen to avoid conflicts with CDC, which uses 0x81, 0x82, and 0x02.

## Testing

The library includes a `QualityAnalysisStream` that can be wired into the RX path to detect audio problems:

```cpp
AudioInfo info(44100, 2, 16);
USBAudioStream in;
QualityAnalysisStream quality(in);
uint8_t buf[1024];

void setup() {
  // Required on cores without built-in TinyUSB support (e.g. mbed RP2040)
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  auto config = in.defaultConfig(RX_MODE);
  config.copyFrom(info);
  in.begin(config);
  quality.setReporting(10000, Serial);
  quality.begin(info);

  // Re-enumerate so the host picks up the new audio interface
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}

void loop() {
  quality.readBytes(buf, sizeof(buf));
}
```

On the host, play a test tone and watch Serial for results:

```bash
sox -n -r 44100 -c 2 -b 16 test_tone.wav synth 5 sine 440
aplay -D hw:Audio test_tone.wav
```

The code is available in the [arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools) library under `src/AudioTools/Communication/USB/`. Examples are in `examples/examples-communication/usb/`.

Feedback and bug reports are welcome — this is a complex protocol stack and real-world testing across different hosts and use cases is invaluable.
