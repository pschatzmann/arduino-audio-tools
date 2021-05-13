#include <WiFi.h>
#include "AudioTools.h"

using namespace audio_tools;

// WIFI
const char *ssid = "ssid";
const char *password = "password";
WiFiServer server(80);
WiFiClient client;

// Sound Generation
const int sample_rate = 10000;
const int data_length = 100000;
const int channels = 1;

SineWaveGenerator<int16_t> sineWave;                      // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in(sineWave, channels);     // Stream generated from sine wave
StreamCopy copier;                                        // buffered copy
WAVEncoder encoder;
AudioOutputStream wav_stream(encoder); // WAV output stream


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Debug);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // start server
  server.begin();

  // start generation of sound
  sineWave.begin(sample_rate, B4);
  in.begin();
}

// Handle an new client connection and return the data
void processClient() {
  if (client)  { // if you get a client,
    Serial.println("New Client."); // print a message out the serial port
    String currentLine = "";       // make a String to hold incoming data from the client
    while (client.connected()) { // loop while the client's connected
      if (client.available()) { // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        Serial.write(c);        // print it out the serial monitor
        if (c == '\n') { // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0){
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:audio/wav");
            client.println();

            // set up wav encoder
            auto config = encoder.defaultConfig();
            config.channels = channels;
            config.sample_rate = sample_rate;
            //config.data_length = data_length;
            config.is_streamed = true;
            encoder.begin(client, config);

            Serial.println("Returning WAV stream...");
            copier.begin(wav_stream, in);
            // break out of the while loop:
            break;
          } else { // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        }
        else if (c != '\r')
        { // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
        }
      }
    }
  }  
}

// copy the data
void loop() {
  if (!client.connected()) {
    client = server.available(); // listen for incoming clients
    processClient();
  } else {
    // We are connected: copy input from source to wav output
    if (encoder){
      copier.copy();
      // if we limit the size of the WAV the encoder gets automatically closed when all has been sent
      if (!encoder) {
        Serial.println("stop client...");
        client.stop();
      }
    }
  }
}