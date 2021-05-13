#include <WiFi.h>
#include "AudioTools.h"

using namespace audio_tools;

const char *ssid = "ssid";
const char *password = "password";
WiFiServer server(80);
WiFiClient client;
AnalogAudioStream microphone;        // analog mic
StreamCopy copier;                   // buffered copy
ConverterAutoCenter<int16_t> center; // The data has a center of around 26427, so we we need to shift it down to bring the center to 0
WAVEncoder encoder;
AudioOutputStream wav_stream(encoder); // WAV output stream
const int sample_rate = 10000;
const int channels = 1;

void setup()
{
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Info);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  AnalogConfig cfg = microphone.defaultConfig(RX_MODE);
  cfg.sample_rate = sample_rate;
  microphone.begin(cfg);

  server.begin();
}


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
            config.data_length = 100000;
            encoder.begin(client, config);

            Serial.println("Returning WAV stream...");
            copier.begin(wav_stream, microphone);
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

void loop() {
  if (!client.connected()) {
    client = server.available(); // listen for incoming clients
    processClient();
  } else {
    // We are connected: copy input from source to wav output
    if (encoder){
      copier.copy(center);
      if (!encoder) {
        Serial.println("stop client...");
        client.stop();
      }
    }
  }
}