/**
 * @author Marco Garzola
 */
#pragma once

#include "mbed.h"

typedef TCPSocket* SOCKET;
typedef UDPSocket* UDPSOCKET;
typedef SocketAddress IPADDRESS;
typedef uint16_t IPPORT;

#define SEND_TMEOUT_MS 1000
#define NULLSOCKET NULL

inline void closesocket(SOCKET s) {
  if (s) {
    s->close();
  }
}

#define getRandom() rand()

inline void socketpeeraddr(SOCKET s, IPADDRESS* addr, IPPORT* port) {
  s->getpeername(addr);
  *port = addr->get_port();
}

inline UDPSOCKET udpsocketcreate(unsigned short portNum) {
  UDPSOCKET s = new UDPSocket();

  if (s->open(NetworkInterface::get_default_instance()) != 0 &&
      s->bind(portNum) != 0) {
    printf("Can't bind port %d\n", portNum);
    delete s;
    return nullptr;
  }
  return s;
}

inline void udpsocketclose(UDPSOCKET s) {
  if (s) {
    s->close();
    delete s;
  }
}

inline ssize_t udpsocketsend(UDPSOCKET sockfd, const void* buf, size_t len,
                             IPADDRESS destaddr, uint16_t destport) {
  if (sockfd) {
    return sockfd->sendto(destaddr.get_ip_address(), destport, buf, len);
  } else {
    return 0;
  }
}
// TCP sending
inline ssize_t socketsend(SOCKET sockfd, const void* buf, size_t len) {
  if (sockfd && buf) {
    sockfd->set_blocking(true);
    sockfd->set_timeout(SEND_TMEOUT_MS);
    return sockfd->send(buf, len);
  } else {
    return 0;
  }
}

inline int socketread(SOCKET sock, char* buf, size_t buflen, int timeoutmsec) {
  if (sock && buf) {
    sock->set_blocking(true);
    sock->set_timeout(timeoutmsec);
    return sock->recv(buf, buflen);
  } else {
    return -1;
  }
}
