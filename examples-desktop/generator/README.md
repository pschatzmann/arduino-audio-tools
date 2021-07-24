# Playing Sound on your Desktop

We provide some generic output which will also work on Linux, Windows and OS/X
The cmake is downloading all dependencies and builds an executable from the sketch.

You just need to provide an Arduino Sketch as cpp file. In our example we use an example setup that can be compiled both in Arduin and with cmake: 

- the sketch is provided as ino file
- the cpp file is including the Arduino.h and the ino file

To build the example execute

```
mkdir build
cd build
cmake ..
make
```

