# BLE Serial

`BLESerialServer` and `BLESerialClient` send and receive data over Bluetooth
Low Energy (BLE) with a simple serial protocol. They are regular audio streams,
so you can use them with `StreamCopy`, `EncodedAudioStream` etc. This is a
data channel, not LE Audio: to keep the throughput low enough, you should
usually send encoded audio (e.g. ADPCM).

| Class | BLE role | Description |
|---|---|---|
| `BLESerialServer` | Peripheral | Advertises the service and waits for a client to connect |
| `BLESerialClient` | Central | Scans for a server with the service UUID and connects to it |

Both sides can send and receive: `write()` sends data to the other side and
`readBytes()` provides the received data.

## Supported BLE libraries

| Implementation | BLE library | Platforms |
|---|---|---|
| `BLESerialServerESP32`, `BLESerialClientESP32` | BLE library of the ESP32 Arduino core (Bluedroid or NimBLE) | ESP32 |
| `BLESerialServerArduinoBLE`, `BLESerialClientArduinoBLE` | [ArduinoBLE](https://github.com/arduino-libraries/ArduinoBLE) | Nano 33 BLE, Nano RP2040 Connect, UNO R4 WiFi, Portenta, ESP32 and others |

`#include "AudioTools/Communication/BLESerial.h"` selects the implementation
for your platform: on the ESP32 `BLESerialServer` and `BLESerialClient` use
the ESP32 BLE library, on all other platforms they use ArduinoBLE. To use
ArduinoBLE on the ESP32, define `BLE_SERIAL_USE_ARDUINOBLE` before the include.
You can also include one of the implementation headers in the `BLESerial`
folder and use the implementation class directly.

## Example

Server that sends audio:

```cpp
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecADPCM.h"
#include "AudioTools/Communication/BLESerial.h"

AudioInfo info(44100, 2, 16);
SineGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
BLESerialServer ble;
ADPCMEncoder adpcm(AV_CODEC_ID_ADPCM_IMA_WAV);
EncodedAudioStream encoder(&ble, &adpcm);
StreamCopy copier(encoder, sound);

void setup() {
  sineWave.begin(info, N_B4);
  encoder.begin(info);
  ble.begin("ble-send");
}

void loop() {
  if (ble.availableForWrite() > 0) copier.copy();
}
```

Client that receives audio:

```cpp
BLESerialClient ble;
...
void setup() {
  ...
  // wait at most 10 minutes for the connection
  ble.begin("ble-receive", 60 * 10);
}

void loop() {
  if (ble) copier.copy();
}
```

See the sketches in `examples/examples-communication/ble`.

## Protocol

The service has two data characteristics (and an optional third one):

| Characteristic | Default UUID | Direction | Setter |
|---|---|---|---|
| Channel 1 | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | server -> client: the client reads it | `setTxUUID()` |
| Channel 2 | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` | client -> server: the client writes it | `setRxUUID()` |
| Audio Info | `6e400004-b5a3-f393-e0a9-e50e24dcca9e` | both: `sample_rate:channels:bits_per_sample` | `setAudioInfoUUID()` |

The service UUID is `6e400001-b5a3-f393-e0a9-e50e24dcca9e` and can be changed
with `setServiceUUID()`.

- The client reads the data from the server: each value starts with a 2 byte
  length (little endian) followed by the data. So the receiving side
  controls the speed in the server -> client direction. This direction
  provides the better throughput.
- The client writes the data to the server: by default without response.
  `setConfirmWrite(true)` requests a response for each write and
  `setWriteThrottle(ms)` waits after each write. If the server can not store
  the data, it is lost.
- `setFramed(true)` (on both sides) keeps the write sizes: each `write()` is
  sent as one BLE packet and `readBytes()` provides one packet at a time.
  This is useful for codecs that need complete frames. A frame must fit into
  one packet (see `packetSize()`).
- `setAudioInfoActive(true)` (on both sides) exchanges the audio info: the
  side that calls `setAudioInfo()` informs the other side, which then calls
  the registered `addNotifyAudioChange()` listeners.

## MTU

The data is sent in packets of at most `packetSize()` bytes: this is the MTU
minus 5 bytes, but at most 510 bytes, because the BLE specification limits a
characteristic value to 512 bytes and the packets read by the client also
contain the 2 byte length. The default MTU is 517 for the ESP32 and 242 for ArduinoBLE.
The ESP32 implementation uses the MTU that was negotiated with the other side.
ArduinoBLE does not report the negotiated MTU: if your BLE controller supports
less than 242 bytes, set the MTU in the constructor or with `setMTU()` before
calling `begin()`.

## Buffers

The received data (and on the server also the data to be sent) is stored in
buffers of 4096 bytes. You can change the size with `setRxBufferSize()` and
`setTxBufferSize()` before calling `begin()`.

With ArduinoBLE the BLE events are processed when you call `readBytes()`,
`available()`, `write()`, `availableForWrite()` or `connected()`: make sure
you call them regularly in your `loop()`.

## Reconnect

When the connection is lost, the client scans for the server again and tries
to reconnect. With ArduinoBLE a connection is considered lost after
`BLE_SERIAL_MAX_READ_ERRORS` (3) failed reads, and a scan that did not find
the server is restarted after `BLE_SERIAL_SCAN_RESTART_MS` (10 s). Avoid
outputs that block the loop for a long time on the receiving side (e.g. a
`CsvOutput` to a USB serial port that is not read by the PC): this can cause
the loss of the connection.
