# I2S input and I2S Output with separate Ports

We use 2 separate I2S Ports to input data from an I2S device and output the data to another I2S Device:

| In   | ESP32 | Out  |
|------|-------|------|
| BCK  | G14   | -    |
| WS   | G15   | -    |
| DATA | G22   | -    |
| -    | G18   | BCK  |
| -    | G19   | WS   |
| -    | G23   | DATA |
| VIN  | VIN   | VIN  |
| GND  | GND   | GND  |
