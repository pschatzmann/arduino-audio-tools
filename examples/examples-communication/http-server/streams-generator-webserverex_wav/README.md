# Webserver

With the help of the ESP32 WIFI functionality we can implement a simple web server. 
In the example we use a Sine Wave generator as sound source and return the result as an WAV file

The server can be used like any other output stream and we can use a StreamCopy to provide it with data.

Multiple users can connect to the server!

## Dependencies

- https://github.com/pschatzmann/arduino-audio-tools
- https://github.com/pschatzmann/TinyHttp.git
