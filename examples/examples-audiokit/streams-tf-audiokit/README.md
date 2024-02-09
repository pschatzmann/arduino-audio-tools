## Using the AI Thinker ESP32 with Tensoflow Lite to Generate Audio

I found some cheap [AI Thinker ESP32 Audio Kit V2.2](https://docs.ai-thinker.com/en/esp32-audio-kit) on AliExpress and because I was tired of all the wires I had to connect to implement my different scenarios that are possible with my [Arduino Audio Tools Library](https://github.com/pschatzmann/arduino-audio-tools), I thought it to be a good idea to buy this board. You dont need to bother about any wires because everything is on one nice board. Just just need to install the dependencies.

<img src="https://pschatzmann.github.io/Resources/img/audio-toolkit.png" alt="Audio Kit" />


The starting point is the good overview provided by [the "Hallo World" example of Tensorflow Lite](https://www.tensorflow.org/lite/microcontrollers/get_started_low_level#train_a_model) which describes how to create, train and use a model which based on the __sine function__. Further information can be found in the [Wiki](https://github.com/pschatzmann/arduino-audio-tools/wiki/Tensorflow-Lite----Audio-Output/_edit)


### Note

The log level has been set to Info to help you to identify any problems. Please change it to AudioLogger::Warning to get the best sound quality!


## Dependencies

You need to install the following libraries:

- https://github.com/pschatzmann/arduino-audio-tools
- https://github.com/pschatzmann/arduino-audio-driver
- https://github.com/pschatzmann/tflite-micro-arduino-examples