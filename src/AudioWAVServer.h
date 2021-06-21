#include <WiFi.h>
#include "AudioTools.h"
#include "CodecWAV.h"

using namespace audio_tools;

/// Calback which writes the sound data to the stream
typedef void (*AudioWAVServerDataCallback)(Stream &out);

/**
 * @brief A simple Arduino Webserver which streams the result as WAV file. 
 * This class is based on the WiFiServer class. All you need to do is to provide the data 
 * with a callback method or from a Stream.
 */
class AudioWAVServer {

public:
    /**
     * @brief Construct a new Audio W A V Server object
     * We assume that the WiFi is already connected
     */
    AudioWAVServer() = default;

    /**
     * @brief Construct a new Audio W A V Server object
     * 
     * @param network 
     * @param password 
     */
    AudioWAVServer(const char* network, const char *password) {
        this->network = network;
        this->password = password;
    }

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

        connectWiFi();

        // start server
        server.begin();
    }

    /**
     * @brief Start the server. The data must be provided by a callback method
     * 
     * @param cb 
     * @param sample_rate 
     * @param channels 
     */
    void begin(AudioWAVServerDataCallback cb, int sample_rate, int channels) {
        this->in =nullptr;
        this->callback = cb;
        this->sample_rate = sample_rate;
        this->channels = channels;

        connectWiFi();

        // start server
        server.begin();
    }

    /**
     * @brief Add this method to your loop
     * Returns true while the client is connected. (The same functionality like doLoop())
     * 
     * @return true 
     * @return false 
     */
    bool copy(){
        return doLoop();
    }

    /**
     * @brief Add this method to your loop
     * Returns true while the client is connected.
     */
    bool doLoop() {
        //LOGD("doLoop");
        bool active = true;
        if (!client.connected()) {
            client = server.available(); // listen for incoming clients
            processClient();
        } else {
            // We are connected: copy input from source to wav output
            if (encoder){
                if (callback==nullptr) {
                    LOGI("copy data...");
                    copier.copy();
                    // if we limit the size of the WAV the encoder gets automatically closed when all has been sent
                    if (!encoder) {
                        LOGI("stop client...");
                        client.stop();
                        active = false;
                    }
                } 
            } else {
                LOGI("client was not connected");
            }
        }
        return active;
    }


protected:
    // WIFI
    WiFiServer server = WiFiServer(80);
    WiFiClient client;
    const char *password = nullptr;
    const char *network = nullptr;

    // Sound Generation
    int sample_rate;
    int channels;

    AudioWAVServerDataCallback callback = nullptr;
    Stream *in = nullptr;                    
    StreamCopy copier;                      
    WAVEncoder encoder;
    AudioOutputStream wav_stream = AudioOutputStream(encoder);  // WAV output stream

    void connectWiFi() {
        LOGD("connectWiFi");
        if (WiFi.status() != WL_CONNECTED && network!=nullptr && password != nullptr){
            WiFi.begin(network, password);
            WiFi.setSleep(false);
            while (WiFi.status() != WL_CONNECTED){
                Serial.print(".");
                delay(500); 
            }
            Serial.println();
        }
        char msg[80];
        sprintf(msg, "IP address: %s",WiFi.localIP().toString().c_str());
        LOGI(msg);
        Serial.println(msg);
    }

    void sendReply(){
        LOGD("sendReply");
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

        // copy only for stream
        if (callback!=nullptr){
            LOGI("sendReply - calling callback");
            // provide data via Callback
            callback(wav_stream);
            client.stop();
        } else {
            LOGI("sendReply - Returning WAV stream...");
            copier.begin(wav_stream, *in);
        }
   }


    // Handle an new client connection and return the data
    void processClient() {
        //LOGD("processClient");
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
