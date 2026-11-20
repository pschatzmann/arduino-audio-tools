
Different optional classes to send and receive audio over the wire or air

| Folder / Header | Description |
|---|---|
| [BLESerial](BLESerial/README.md) | Serial data channel via Bluetooth Low Energy (ESP32 BLE or ArduinoBLE) |
| `AudioBLE.h` | LE Audio (unicast) sink (RX_MODE) and source (TX_MODE) as an AudioStream, ESP32 targets with LE Audio (e.g. ESP32-S31) using the core's BLEAudio library |
| [AudioPlayer](AudioPlayer/README.md) | Protocols to control the AudioPlayer remotely |
| [HTTP](HTTP/README.md) | HTTP client and server |
| [Network](Network/README.md) | Network abstraction |
| [USB](USB/README.md) | USB Audio device and host |
| `A2DPStream.h` | `A2DPStream`: Bluetooth A2DP source or sink (ESP32) as an AudioStream |
| `AdaptiveResamplingStream.h`, `AdaptiveResamplingBuffer.h` | `AdaptiveResamplingStream`: buffer which corrects the sample rate drift between producer and consumer; `AdaptiveResamplingBuffer` is its buffer |
| `AudioHttp/`, `AudioHttp.h` | Convenience header which includes the networking functionality (HTTP server and client) |
| `AudioLoRa.h` | `AudioLoRa`: audio over LoRa radio |
| `AudioServerEx.h` | `AudioServerEx` and `AudioWAVServerEx`: audio servers which send the audio to HTTP clients |
| `AudioSync.h` | `AudioSyncWriter` and `AudioSyncReader`: keep the amount of data between writer and reader synchronized |
| `ESPNowStream.h` | `ESPNowStream`: audio over ESP-NOW (ESP32) |
| `FEC/`, `HammingFEC.h`, `ReedSolomonFEC.h` | Forward error correction: `HammingFEC` and `ReedSolomonFEC` (write encodes, read decodes) |
| `HDLCStream.h` | `HDLCStream`: HDLC framing (bit-oriented, code-transparent) of the data |
| `HLSStream.h`, `HLSStreamESP32.h` | `HLSStream`: HTTP Live Streaming (HLS) client |
| `HTTP/` | `URLStream`, `ICYStream`, `AudioServer`, `AudioWAVServer` and related helpers (see its README) |
| `Network/` | Network classes for WiFi, Ethernet and UDP on the different platforms (see its README) |
| `NRF24Stream.h` | `NRF24Stream`: low-latency audio over NRF24L01 radios |
| `ObjectStream.h` | `ObjectStream`: a Stream which makes sure that the same size is read back as was written |
| `OSCData.h` | `OSCData`: binary data structure used by the OSC protocol |
| `RadioHeadStream.h` | `RadioHeadStream`: audio over the RadioHead library |
| `RCAudioPlayerOSC.h` | `RCAudioPlayerOSCSender` and `RCAudioPlayerOSCReceiver`: remote control of the AudioPlayer via OSC |
| `RedisBuffer.h` | `RedisBuffer`: buffer which stores and retrieves the data in a Redis database |
| `RTSP/`, `RTSP.h`, `RTSPTaskless.h` | RTSP server and client (audio and video) |
| `SPI/` | `SPIAudioMaster`, `SPIAudioSlave`, `SPIRegAudioMaster` and `SPIRegAudioSlave`: audio over SPI |
| `UDPStream.h` | `UDPStream`: UDP as an AudioStream which makes sure that the packets are handled consistently |
| `VBAN/`, `VBANStream.h` | `VBANStream` and `VBANConfig`: VBAN audio source and sink (ESP32) |
| `WebSocketOutput.h` | `WebSocketOutput`: AudioOutput which sends the data via WebSocket |
