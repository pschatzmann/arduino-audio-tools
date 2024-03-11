# ESP32 Analog to Digital Conversion Throughput using Continuous ADC API

Data is reported in bytes per second when using a single ADC unit and two A/D channels.
When each channel is measured with 4,000 samples per second the ADC is running at 8,000 samples per second as it multiplexes between the two channels.

The maximum sampling frequency depends on the ESP32 model.

The older ESP32 models can convert up to 2 Million samples per second, however there is discrepancy between the expected and effective throughput of about 20%.

The library creates int16_t datatype which is 2 bytes per sample.

## Adafruit Feather ESP32-S3 2MB PSRAM
```
    
   SPS  :  expected,  measured
      150: error, sample rate eff: 300, range: 611 to 83,333
      306:    1,224,     1,000
    4,000:   16,000,    16,000
    8,000:   32,000,    32,000   
   11,025:   44,100,    44,000
   16,000:   64,000,    64,000
   20,000:   80,000,    80,000
   22,050:   88,200,    89,000
   40,000:  160,000,   161,000
   44,100: error, sample rate eff: 88,200, range: 611 to 83,333
```

## Sparkfun ESP32 WROOM Plus C
```
  Data: bytes per second, single unit, two channels
   SPS/CH  expected,  measured % of expected
    5000:  error, sample rate eff: 10000, range: 20000 to 2000000
   10000:    40,000,    32,000
   22050:    88,200,    72,000
   44100:   176,400,   144,000
   88200:   352,800,   157,000
  500000: 2,000,000, 1,635,000
 1000000: 4,000,000, 3,271,000
```

## DOIT ESP32 DEVKIT V1
```
  Data: bytes per second, single unit, two channels
   SPS  :  expected,  measured 80% of expected
    5000:  error, sample rate: 5000, range: 10000 to 1000000
   10000:    40,000,    32,000
   20000:    80,000,    64,000
   22050:    88,200,    72,000
   44100:   176,400,   144,000
   48000:   192,000,   156,000
   88200:   352,800,   288,000
   96000:   384,000,   313,000
  176400:   705,600,   576,000
  192200:   768,800,   628,000
  352800: 1,411,200, 1,153,000
  384000: 1,536,000, 1,256,000
  500000: 2,000,000, 1,635,000
 1000000: 4,000,000, 3,271,000
 1500000: error, sample rate: 1500000, range: 10000 to 1000000
```