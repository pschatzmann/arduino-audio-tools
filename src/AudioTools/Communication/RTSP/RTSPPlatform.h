/*
 * Author: Phil Schatzmann
 *
 * Based on Micro-RTSP library:
 * https://github.com/geeksville/Micro-RTSP
 * https://github.com/Tomp0801/Micro-RTSP-Audio
 *
 */
#pragma once

#include <Arduino.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace audio_tools {

/**
 * @brief Template-based platform abstraction for RTSP networking
 *
 * This template class provides a unified interface for network operations
 * across different platforms while maintaining type safety. The template
 * parameters allow customization of the underlying network implementation.
 * @ingroup rtsp
 * @author Phil Schatzmann
 * 
 * @tparam TcpClient TCP client implementation (e.g., WiFiClient,
 * EthernetClient)
 * @tparam UdpSocket UDP socket implementation (e.g., WiFiUDP, EthernetUDP)
 */
template <typename TcpServer, typename TcpClient, typename UdpSocket>
class RTSPPlatform {
 public:
  // Type aliases for template parameters
  using TcpServerType = TcpServer;
  using TcpClientType = TcpClient;
  using UdpSocketType = UdpSocket;
  /**
   * @brief Create a TCP server listening on provided port
   */
  static TcpServerType* createServer(uint16_t port){
    TcpServerType *srv = new TcpServerType(port);
    srv->begin();
    return srv;
  }

  /**
   * @brief Get next available client from server
   */
  static TcpClientType getAvailableClient(TcpServerType *srv){
    return srv->accept();
  }
  
  static constexpr TcpClient* NULL_TCP_SOCKET = nullptr;
  static constexpr UdpSocket* NULL_UDP_SOCKET = nullptr;

  /**
   * @brief Close a TCP socket connection
   * @param s TCP socket to close
   */
  static void closeSocket(TcpClient* s) {
    if (s) {
      s->stop();
      // delete s; // TCP clients are typically not heap-allocated in Arduino
    }
  }

  /**
   * @brief Get remote peer address and port from TCP socket
   * @param s TCP socket
   * @param addr Output parameter for remote IP address
   * @param port Output parameter for remote port
   */
  static void getSocketPeerAddr(TcpClient* s, IPAddress *addr,
                             uint16_t *port) {
    // For WiFiClient and compatible types, assume remoteIP/remotePort are
    // available
    if (s) {
      *addr = s->remoteIP();
      *port = s->remotePort();
    } else {
      *addr = IPAddress();
      *port = 0;
    }
  }

  /**
   * @brief Close a UDP socket
   * @param s UDP socket to close
   */
  static void closeUdpSocket(UdpSocket* s) {
    if (s) {
      s->stop();
      delete s;
    }
  }

  /**
   * @brief Create and bind a UDP socket
   * @param portNum Port number to bind to
   * @return Pointer to created UDP socket, or nullptr on failure
   */
  static UdpSocket* createUdpSocket(unsigned short portNum) {
    UdpSocket* s = new UdpSocket();

    if (!s->begin(portNum)) {
      printf("Can't bind port %d\n", portNum);
      delete s;
      return nullptr;
    }

    return s;
  }

  /**
   * @brief Send data over TCP socket
   * @param sockfd TCP socket
   * @param buf Data buffer to send
   * @param len Length of data
   * @return Number of bytes sent
   */
  static ssize_t sendSocket(TcpClient* sockfd, const void *buf, size_t len) {
    return sockfd->write((uint8_t *)buf, len);
  }

  /**
   * @brief Send UDP packet to specified destination
   * @param sockfd UDP socket
   * @param buf Data buffer to send
   * @param len Length of data
   * @param destaddr Destination IP address
   * @param destport Destination port
   * @return Number of bytes sent
   */
  static ssize_t sendUdpSocket(UdpSocket* sockfd, const void *buf, size_t len,
                               IPAddress destaddr, uint16_t destport) {
    sockfd->beginPacket(destaddr, destport);
    sockfd->write((const uint8_t *)buf, len);
    if (!sockfd->endPacket()) printf("error sending udp packet\n");

    return len;
  }

  /**
   * @brief Read from TCP socket with timeout
   * @param sock TCP socket
   * @param buf Buffer to read into
   * @param buflen Buffer length
   * @param timeoutmsec Timeout in milliseconds
   * @return 0=socket closed, -1=timeout, >0=number of bytes read
   */
  static int readSocket(TcpClient* sock, char *buf, size_t buflen,
                        int timeoutmsec) {
    if (!sock->connected()) {
      printf("client has closed the socket\n");
      return 0;
    }

    int numAvail = sock->available();
    if (numAvail == 0 && timeoutmsec != 0) {
      // sleep and hope for more
      delay(timeoutmsec);
      numAvail = sock->available();
    }

    if (numAvail == 0) {
      // printf("timeout on read\n");
      return -1;
    } else {
      // int numRead = sock->readBytesUntil('\n', buf, buflen);
      int numRead = sock->readBytes(buf, buflen);
      // printf("bytes avail %d, read %d: %s", numAvail, numRead, buf);
      return numRead;
    }
  }
};


}  // namespace audio_tools
