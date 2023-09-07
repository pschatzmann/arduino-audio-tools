# Test Signal to Bluetooth Speaker

Sometimes it is quite useful to be able to generate a test tone.
We can use the GeneratedSoundStream class together with a SoundGenerator class. In my example I use a SineWaveGenerator.

To test the output I'm using this generated signal and write it to A2DP (e.g. a Bluetooth Speaker).

### Dependencies

- https://github.com/pschatzmann/ESP32-A2DP.git
