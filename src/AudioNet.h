#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>

namespace audio_tools {

/**
 * @brief Just an alternative name for EthernetClient - To be consistent with the other Stream classes we support begin and end on top of the
 * standard connect and end methods.
 * 
 */

class IPStream : public WiFiClient {
    public:
        uint8_t begin(IPAddress remote_host, uint16_t port) {
            active = connect(remote_host, port);
            return active;
        }

        virtual void end() {
            stop();
            active = false;
        }      

        operator bool() {
            return active;
        }

    protected:
        bool active;


};

/**
 * @brief The same as EthernetUDP with the difference that we indicate the destination host in the begin and not 
 * for each packet write
 */
class UDPStream : public WiFiUDP {
    public:

        virtual uint8_t begin(IPAddress remote_host, uint16_t remote_port, uint16_t local_port=0, bool multicast = false ) {
            LOGI( "begin");
            this->remote_host = remote_host;
            this->remote_port = remote_port;
            this->local_port = local_port != 0 ? local_port : remote_port;

            if (multicast){
                active = WiFiUDP::beginMulticast(remote_host, remote_port);
            } else {
                active = WiFiUDP::begin(local_port);
            }

            return active;
        }

        virtual void end() {
            WiFiUDP::stop();
            active = false;
        }      

        virtual size_t write(const uint8_t *buffer, size_t size) {
            LOGD( "write %u bytes", size);
            beginPacket(remote_host, remote_port);
            size_t result = WiFiUDP::write(buffer, size);
            endPacket();
            return result;
        }

        operator bool() {
            return active;
        }

    protected:
        IPAddress remote_host;
        uint16_t remote_port;
        uint16_t local_port;
        bool active;
};

}