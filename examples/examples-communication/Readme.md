# Communication Examples

You can transmit audio information over a wire or (e.g. if you have an ESP32) wirelessly. It is important that you make sure that the transmitted amount of audio data is below the transmission capacity of the medium (e.g. by limiting the channels, the sample rate or the bits per sample). 

You can also use [one of the many supported CODECs](https://github.com/pschatzmann/arduino-audio-tools/wiki/Encoding-and-Decoding-of-Audio) to decrease the transmitted amount of data!

## SPI

`SPIRegAudioMaster` and `SPIRegAudioSlave` transmit audio over SPI. The slave requires the [TinySPISlave](https://github.com/pschatzmann/TinySPISlave) library (ESP32, RP2040/RP2350, STM32), which you need to install; the master does not need it and is controlled by the master via a small register map:

| Address | Register | Access | Content |
|---|---|---|---|
| 0x01 | AUDIO_INFO | rw | sample rate (u32), channels (u16), bits per sample (u8), reserved |
| 0x02 | MIME | rw | 32 byte zero terminated mime type |
| 0x03 | AVAILABLE | ro | u32 free space in the slave buffer (flow control) |
| 0x10 | DATA | wo | audio data which is appended to the slave buffer |

The master sets the AudioInfo and mime type and writes the data according to the available buffer space. On the slave a `MultiDecoder` selects the decoder (MP3, AAC, WAV) from the mime type; `audio/pcm` bypasses the decoder. See the `spi/spi-reg-master` and `spi/spi-reg-slave` examples.
