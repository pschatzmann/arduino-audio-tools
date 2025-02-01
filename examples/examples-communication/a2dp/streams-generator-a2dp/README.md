# Test Signal to Bluetooth Speaker

Sometimes it is quite useful to be able to generate a test tone.
We can use the GeneratedSoundStreamT class together with a SoundGeneratorT class. In my example I use a SineWaveGeneratorT.

To test the output I'm using this generated signal and write it to A2DP (e.g. a Bluetooth Speaker).

### Dependencies

- https://github.com/pschatzmann/ESP32-A2DP.git
