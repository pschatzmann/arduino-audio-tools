#include <WiFi.h>
#include "AudioTools.h"
#include "CodecWAV.h"

using namespace audio_tools;

/**
 * @brief ESP32 Webserver which streams the result as WAV file. 
 * 
 */
class AudioWAVServer {

public:
    AudioWAVServer() = default;
    /**
     * @brief Start the server. You need to be connected to WiFI before calling this method
     * 
     * @param in 
     * @param sample_rate 
     * @param channels 
     */
    void begin(Stream &in, int sample_rate, int channels) {
        this->in = &in;
        this->sample_rate = sample_rate;
        this->channels = channels;

        LOGI("IP address: %s",WiFi.localIP().toString().c_str());

        // start server
        server.begin();
    }

    /**
     * @brief Add this method to your loop
     * 
     */
    void copy() {
        if (!client.connected()) {
            client = server.available(); // listen for incoming clients
            processClient();
        } else {
            // We are connected: copy input from source to wav output
            if (encoder){
                copier.copy();
                // if we limit the size of the WAV the encoder gets automatically closed when all has been sent
                if (!encoder) {
                    LOGI("stop client...");
                    client.stop();
                }
            }
        }
    }

protected:
    // WIFI
    WiFiServer server = WiFiServer(80);
    WiFiClient client;

    // Sound Generation
    int sample_rate;
    int channels;

    Stream *in;                             // Stream generated from sine wave
    StreamCopy copier;                      // buffered copy
    WAVEncoder encoder;
    AudioOutputStream wav_stream = AudioOutputStream(encoder);  // WAV output stream


    void sendReply(){
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

        LOGI("Returning WAV stream...");
        copier.begin(wav_stream, *in);
   }


    // Handle an new client connection and return the data
    void processClient() {
        if (client)  { // if you get a client,
            LOGI("New Client."); // print a message out the serial port
            String currentLine = "";       // make a String to hold incoming data from the client
            while (client.connected()) { // loop while the client's connected
                if (client.available()) { // if there's bytes to read from the client,
                    char c = client.read(); // read a byte, then
                    if (c == '\n') { // if the byte is a newline character

                        // if the current line is blank, you got two newline characters in a row.
                        // that's the end of the client HTTP request, so send a response:
                        if (currentLine.length() == 0){
                            sendReply();
                            // break out of the while loop:
                            break;
                        } else { // if you got a newline, then clear currentLine:
                            currentLine = "";
                        }
                    }
                    else if (c != '\r') { // if you got anything else but a carriage return character,
                        currentLine += c; // add it to the end of the currentLine
                    }
                }
            }
        }  
    }

};
