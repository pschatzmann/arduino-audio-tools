# Minimum Desktop Runtime

We provide some generic output which will also work on Linux, Windows and OS/X
The cmake is downloading all dependencies and builds an executable from the sketch.

This example is not using the Arduino-Emulator!

You just need to provide an Arduino Sketch as cpp file. In our example we use an example setup that can be compiled both in Arduin and with cmake: 

- the sketch is provided as ino file
- you must not include Arduino.h since this is not available

To build the example execute

```
mkdir build
cd build
cmake ..
make
```

