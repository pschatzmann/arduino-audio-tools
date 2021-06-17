# Decoding a WAV file

In this example we decode a WAV file into RAW output and send it to a PWM pin on a Raspberry Pico
 
MemoryStream -> AudioOutputStream -> WAVDecoder -> AudioPWM

For the time beeing we just have an impelemntation of AudioPWM for the Rasperry Pico. But it should be possible to extend in to others as well in the future....