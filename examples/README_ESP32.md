# ESP32 Configuration

If you are using ESP32 by Espressif Systems version 3.0.0 and later, audio tools will use the new adc_continuous API.

For the adc_continuous API, audio tools provides the following options:

- **sample_rate** (per channel), the effective ADC sampling rate is the number of channels x sample rate and can be:
    - ESP32: 20kHz to 2MHz
    - ESP32 S2, S3, H2, C6, C3: 611Hz to 83.333kHz

for example:

    -       306
    -     4,000
    -     5,000
    -     8,000
    -    10,000
    -    11,025
    -    16,000
    -    20,000
    -    22,050
    -    40,000
    -    44,100
    -    48,000
    -    88,200
    -    96,000
    -   176,400
    -   192,200
    -   352,800
    -   384,000
    -   500,000
    - 1,000,000

- **adc_bit_with**
    - 9, 10, 11, 12 depending on ESP32 model
    - audio stream is int16_t
- **adc_calibration_active**: values measured are in mV
- **is_auto_center_read**: subtraction of current estimated average from samples
- **adc_attenuation**:

    | attenuation     | range   | accurate range |
    | ------------    | --------| -------------- |
    | ADC_ATTEN_DB_0  | 0..1.1V | 100-950mV      |
    | ADC_ATTEN_DB_2_5| 0..1.5V | 100-1250mV     |
    | ADC_ATTEN_DB_6  | 0..2.2V | 150-1750mV     |
    | ADC_ATTEN_DB_12 | 0..3.9V | 150-2450mV     |
   
- **channels**:
    - mono = 1  
    - stereo = 2
- **adc_channels**: defining the channels (only channels on ADC unit 1 are supported) e.g.:
    - A3 on Sparkfun ESP32 Thing Plus is ADC_CHANNEL_3
    - A4 on Sparkfun ESP32 Thing Plus is ADC_CHANNEL_0
    - D5 on Adafruit ESP32-S3 is ADC_CHANNEL_4
    - D6 on Adafruit ESP32-S3 is ADC_CHANNEL_5

- **buffer_size**
    - maximum is 2048
    - minimum is number of channels
    - number needs to be devisible by number of channels
    - care must be taken because some streams in audio tools can not exceed 1024 bytes

## Example Configuration
```
auto adcConfig = adc.defaultConfig(RX_MODE);
adcConfig.sample_rate = 44100;
adcConfig.adc_bit_width = 12;
adcConfig.adc_calibration_active = true;
adcConfig.is_auto_center_read = false;
adcConfig.adc_attenuation = ADC_ATTEN_DB_12; 
adcConfig.channels = 2;
adcConfig.adc_channels[0] = ADC_CHANNEL_4; 
adcConfig.adc_channels[1] = ADC_CHANNEL_5;
```

## ADC unit 1 channels on common ESP32 boards
Audio tools continous ADC framewaork supports ADC Unit 1 only.

### Sparkfun ESP32 Thing Plus (ESP32)
- A2, ADC1_CH6
- A3, ADC1_CH3 
- A4, ADC1_CH0 
- 32, ADC1_CH4
- 33, ADC1_CH5

### Sparkfun ESP32 Thing Plus C (ESP32)
- A2, ADC1_CH6
- A3, ADC1_CH3 
- A4, ADC1_CH0 
- A5, ADC1_CH7
- 32/6, ADC1_CH4
- 33/10, ADC1_CH5

### Sparkfun ESP32 Qwiic Pocket Development (ESP32C6)
- 2, ADC1_CH2
- 3, ADC1_CH3
- 4, ADC1_CH4
- 5, ADC1_CH5

### ESP32-C6-DevKit
- 4, ADC1_CH4
- 5, ADC1_CH5
- 6, ADC1_CH6
- 7, ADC1_CH0
- 0, ADC1_CH1
- 2, ADC1_CH2
- 3, ADC1_CH3

### ESP32-H2-DevKit
- 1, ADC1_CH0
- 2, ADC1_CH1
- 3, ADC1_CH2
- 4, ADC1_CH3
- 5, ADC1_CH4

### ESP32­-S3­-DevKit
- 1,  ADC1_CH0
- 2,  ADC1_CH1
- 4,  ADC1_CH3
- 5,  ADC1_CH4
- 6,  ADC1_CH5
- 7,  ADC1_CH6
- 8,  ADC1_CH7
- 3,  ADC1_CH2
- 9,  ADC1_CH8
- 10, ADC1_CH9

### ESP32-C3-DevKit
-  4, IO2, ADC1_CH2
-  5, IO3, ADC1_CH3
-  9, IO0, ADC1_CH0
- 10, IO1, ADC1_CH1
- 11, IO4, ADC1_CH4

### Adafruit ESP32 Feather V2
- D32, ADC1_CH4
- D33, ADC1_CH5
-  A2, ADC1_CH6
-  A3, ADC1_CH3
-  A4, ADC1_CH0
- D37, ADC1_CH1

### Adafruit ESP32-S3 Feather
-  D5, ADC1_CH4
-  D6, ADC1_CH5
-  D9, ADC1_CH8
- D10, ADC1_CH9
-  A5, ADC1_CH7

### Adafruit QT Py ESP32-C3
- A0, ADC1-CH4
- A1, ADC1-CH3
- A2, ADC1-CH1
- A3, ADC1-CH0
