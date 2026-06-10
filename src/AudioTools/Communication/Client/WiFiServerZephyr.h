#pragma once

#include <cstring>

#include <zephyr/net/socket.h>

namespace audio_tools {

/**
 * WiFiServer (Zephyr zsock version)
 */
class WiFiServerZephyr {
 public:
  explicit WiFiServerZephyr(int port)
      : port(port),
        listen_sock(-1) {}

  ~WiFiServerZephyr() { end(); }
  /**
   * Start listening socket
   */
  void begin() {
    listen_sock = zsock_socket(AF_INET,
                               SOCK_STREAM,
                               IPPROTO_TCP);

    if (listen_sock < 0) {
      return;
    }

    int opt = 1;

    zsock_setsockopt(listen_sock,
                      SOL_SOCKET,
                      SO_REUSEADDR,
                      &opt,
                      sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    zsock_bind(listen_sock,
               (sockaddr *)&addr,
               sizeof(addr));

    zsock_listen(listen_sock,
                 5);
  }

  /**
   * Accept a new client (non-blocking safe)
   */
  WiFiClient available() {
    if (listen_sock < 0) {
      return WiFiClient();
    }

    sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);

    int s = zsock_accept(listen_sock,
                         (sockaddr *)&client_addr,
                         &len);

    if (s < 0) {
      return WiFiClient();
    }

    return WiFiClient(s);
  }

  WiFiClient accept() {
    return available();
  }

  void end() {
    if (listen_sock >= 0) {
      zsock_close(listen_sock);
      listen_sock = -1;
    }
  }

  int portNumber() const {
    return port;
  }

 private:
  int port;
  int listen_sock;
};

using WiFiServer = WiFiServerZephyr;

} // namespace zephyr_arduino