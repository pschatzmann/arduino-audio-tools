# Display Generated Sound

Sometimes it is quite useful to be able to generate a test tone.
We can use the GeneratedSoundStream class together with a SoundGenerator class. In my example I use a SineWaveGenerator.

In order to make sure that we use a consistent data type in the sound processing I also used a ```typedef int16_t sound_t;```. You could experiment with some other data types.

To be able to verify the result I used ```CsvStream<sound_t> printer(Serial, channels);``` which generates CSV output and sends it to Serial.

Here is the result on the Arduino Serial Plotter:

![serial-plotter](https://pschatzmann.github.io/arduino-audio-tools/resources/serial-plotter-sine.png)
