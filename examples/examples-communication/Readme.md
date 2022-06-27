
You can transmit audio information with a wire or (e.g. if you have an ESP32) wirelessly. The critical piece is that you make sure that the transmitted amount of audio data is below the transmission capacity of the medium (e.g. by limiting the channels, the sample rate or the bits per sample). 

You can also use a CODEC to decrease the transmitted amount of data!