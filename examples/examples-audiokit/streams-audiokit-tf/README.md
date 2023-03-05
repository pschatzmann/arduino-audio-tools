## Using the AI Thinker ESP32 Audio Kit Micro Speech

I found some cheap [AI Thinker ESP32 Audio Kit V2.2](https://docs.ai-thinker.com/en/esp32-audio-kit) on AliExpress and because I was tired of all the wires I had to connect to implement my different scenarios that are possible with my [Arduino Audio Tools Library](https://github.com/pschatzmann/arduino-audio-tools), I thought it to be a good idea to buy this board.

<img src="https://pschatzmann.github.io/Resources/img/audio-toolkit.png" alt="Audio Kit" />

You dont need to bother about any wires because everything is on one nice board. Just just need to install the dependencies

The staring point for doing speech recognition on an Arduino based board is TensorFlow Light For Microcontrollers with the example sketch called micro_speech!

I have adapted the MicroSpeech example from TensorFlow Lite to follow the philosophy of this framework. The example uses a Tensorflow model which can recognise the words 'yes' and 'no'. The output stream class is TfLiteAudioOutput. In the example I am using an ESP32 AudioKit board, but you can replace this with any type of processor with a microphone.
Further information can be found in the [Wiki](https://github.com/pschatzmann/arduino-audio-tools/wiki/TensorFlow-Lite---MicroSpeech)


### Note

The log level has been set to Info to help you to identify any problems. Please change it to AudioLogger::Warning to get the best sound quality!


## Dependencies

You need to install the following libraries:

- https://github.com/pschatzmann/arduino-audio-tools
- https://github.com/pschatzmann/arduino-audiokit
- https://github.com/pschatzmann/tflite-micro-arduino-examples