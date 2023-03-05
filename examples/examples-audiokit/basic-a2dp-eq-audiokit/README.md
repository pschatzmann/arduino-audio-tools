## Using the AI Thinker ESP32 Audio Kit as A2DP Receiver with Equilizer

I found some cheap [AI Thinker ESP32 Audio Kit V2.2](https://docs.ai-thinker.com/en/esp32-audio-kit) on AliExpress.

<img src="https://pschatzmann.github.io/Resources/img/audio-toolkit.png" alt="Audio Kit" />

I am using the data callback of the A2DP library to feed the AudioKitStream that will be controlled by an Equilizer

You dont need to bother about any wires because everything is on one nice board. Just just need to install the dependencies:

## Dependencies

You need to install the following libraries:

- https://github.com/pschatzmann/arduino-audio-tools
- https://github.com/pschatzmann/ESP32-A2DP
- https://github.com/pschatzmann/arduino-audiokit
