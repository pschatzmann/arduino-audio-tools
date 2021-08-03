# Decoding a WAV file

In this example we decode a WAV file into RAW output and send it to a PWM pins (on a Raspberry Pico or ESP32)

The WAV file has been down sampled with the help of __Audacity__ and then converted into an array with __xxd__. 

Please note that this example is still using the more confusing old api which uses the following processing chain:
MemoryStream -> AudioOutputStream -> WAVDecoder -> AudioPWM 


The pins depend on the Processor:

| PIEZO   |  ESP32       | RPI Pico      | MBED         |
| --------| -------------|---------------|--------------|
| +       |  GPIO4       | GPIO2         | GPIO2        |
| -       |  GND         |               |              |


Complie with Partition Scheme Huge APP!