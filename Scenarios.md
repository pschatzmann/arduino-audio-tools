# Schenarios

### Supported Architectures

Unfortunatly Arduino does not provide an I2S functionality which is standrdized acress the different processors. There is only an official documentation for SAMD21 processors. The full functionality of the library is currently only available on the ESP32: 


| Processor      | I2S       | ADC/DAC  | A2DP   | URLStream | PWM   | Other  |
|----------------|-----------|----------|--------|-----------|-------|--------|
| ESP32          |  +        |  +       |   +    |   +       |   *   |   +    |
| ESP8266        |  *        |  *       |        |           |       |   +    |
| SAMD21         |  *        |          |        |           |       |   +    |
| Raspberry Pico |           |          |        |           |   *   |   +    |
 

+ supported
* not tested


### Supported Schenarios

Here are the related Stream classes with their supported operations that can be used:

| Class                   | Read | Write | Comments           |
|-------------------------|------|-------|--------------------|
| I2SStream               |   +  |   +   | i2s                |
| PWMAduioStream          |      |   +   | pwm                |
| AnalogAudioStream       |   +  |   +   | adc, dac           |
| MemoryStream            |   +  |   +   | memory             |
| URLStream               |   +  |       | url                |
| A2DPStream              |   +  |   +   | a2dp               |
| GeneratedSoundStream    |   +  |       | gen                |
| AudioOutputWithCallback |   +  |   +   |                    |
| CsvStream               |      |   +   | csv                |
| File                    |   +  |   +   | From SD library    |
| Serial, ...             |   +  |   +   | Std Arduino        |



In theory we should be able to support the following scenarios:

| Input  | dac  | i2s | file | a2dp | Serial | csv  |
|--------|------|-----|------|------|--------|------|
| adc    |  +   |  +  |   +  |  +   |   +    |  +   |
| i2s    |  +   |  +  |   +  |  +   |   +    |  +   |
| file   |  +   |  +  |   +  |  +   |   +    |  +   |
| a2dp   |  +   |  +  |   +  |  -   |   +    |  +   |
| Serial |  +   |  +  |   +  |  +   |   +    |  +   |
| memory |  +   |  +  |   +  |  +   |   +    |  +   |
| url    |  +   |  +  |   +  |  -   |   +    |  +   |
| gen    |  +   |  +  |   +  |  +   |   +    |  +   |



