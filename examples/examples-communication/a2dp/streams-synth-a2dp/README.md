# A Simple Synthesizer for the AI Thinker AudioKit to A2DP Bluetooth Speaker

I was taking the streams-synth-audiokit example and converted the output to go to A2DP. In order to minimize the lag
I am not using the Stream API but directly the A2DP Callbacks - which avoids any buffers.

The delay howver is still too big to be really useful...

### Dependencies

You need to install the following libraries:

- [Arduino Audio Tools](https://github.com/pschatzmann/arduino-audio-tools)
- [Midi](https://github.com/pschatzmann/arduino-midi)
- [ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP)

