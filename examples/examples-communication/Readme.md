# Communication Examples

You can transmit audio information over a wire or (e.g. if you have an ESP32) wirelessly. It is important that you make sure that the transmitted amount of audio data is below the transmission capacity of the medium (e.g. by limiting the channels, the sample rate or the bits per sample). 

You can also use [one of the many supported CODECs](https://github.com/pschatzmann/arduino-audio-tools/wiki/Encoding-and-Decoding-of-Audio) to decrease the transmitted amount of data!