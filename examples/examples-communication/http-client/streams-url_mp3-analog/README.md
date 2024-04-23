# Streaming Radio Player

We just play a streamed radio station which provides the audio as mp3 and output the result via I2S via the built in DAC

An ESP32 was used to test this sketch.

### Output Device: Piezo Electric Element

To test the output I am using a piezo electric element

![DAC](https://pschatzmann.github.io/Resources/img/piezo.jpeg)

It should also be possible to connect a headphone to the output pins...


On the ESP32 the output is on the Pins GPIO26 and GPIO27

| PIEZO     |  ESP32
| --------| ---------------
| +       |  GPIO25 / GPIO26
| -       |  GND


