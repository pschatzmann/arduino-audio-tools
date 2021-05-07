# Digital output via I2S to a external DAC

Sometimes it is quite useful to be able to generate a test tone.
We can use the GeneratedSoundStream class together with a SoundGenerator class. In my example I use a SineWaveGenerator.

To test the I2S output I'm using this generated signal and write it to A2DP (e.g. a Bluetooth Speaker).

