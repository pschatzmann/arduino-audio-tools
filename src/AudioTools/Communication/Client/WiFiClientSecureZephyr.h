#pragma once
#include "WiFiClientZephyr.h"
#include <zephyr/net/tls_credentials.h>  // sec_tag_t, tls_credential_type, tls_credential_add, TLS_CREDENTIAL_*
#include <zephyr/net/socket.h>           // SOL_TLS, TLS_PEER_VERIFY, TLS_HOSTNAME, TLS_SEC_TAG_LIST, IPPROTO_TLS_1_2

namespace audio_tools {

/**
 * WiFiClientSecure-compatible TLS/TCP client for Zephyr RTOS.
 * Header-only implementation — just #include this file.
 *
 * Typical usage:
 *   WiFiClientSecureZephyr client;
 *   client.setCACert(ca_pem, ca_pem_len);          // optional: verify server
 *   client.setCertificate(cert_pem, cert_pem_len); // optional: mutual TLS
 *   client.setPrivateKey(key_pem, key_pem_len);    // optional: mutual TLS
 *   client.connect("example.com", 443);
 */

class WiFiClientSecureZephyr : public WiFiClientZephyr {
 public:
  WiFiClientSecureZephyr()
      : WiFiClientZephyr(),   // base initialises _sock, _timeout_ms, etc.
        _insecure(false),
        _ca_tag(TLS_TAG_NONE),
        _cert_tag(TLS_TAG_NONE),
        _key_tag(TLS_TAG_NONE),
        _next_tag(TAG_BASE) {}

  ~WiFiClientSecureZephyr() { stop(); }  // calls base stop()

  // --- credential API (unchanged from before) ---
  bool setCACert(const uint8_t* pem, size_t len)      { return _addCredential(TLS_CREDENTIAL_CA_CERTIFICATE,   pem, len, &_ca_tag);   }
  bool setCACert(const char* pem)                      { return pem && setCACert((const uint8_t*)pem, strlen(pem)+1); }
  bool setCertificate(const uint8_t* pem, size_t len) { return _addCredential(TLS_CREDENTIAL_PUBLIC_CERTIFICATE, pem, len, &_cert_tag); }
  bool setCertificate(const char* pem)                 { return pem && setCertificate((const uint8_t*)pem, strlen(pem)+1); }
  bool setPrivateKey(const uint8_t* pem, size_t len)  { return _addCredential(TLS_CREDENTIAL_PRIVATE_KEY,      pem, len, &_key_tag);  }
  bool setPrivateKey(const char* pem)                  { return pem && setPrivateKey((const uint8_t*)pem, strlen(pem)+1); }
  void setInsecure()                                   { _insecure = true; }

  // --- only connect() needs overriding ---
  int connect(const char* host, uint16_t port) override {
    stop();  // inherited

    struct sockaddr_storage addr = {};
    socklen_t addrlen = sizeof(addr);

    if (_resolve(host, port, (struct sockaddr*)&addr, &addrlen) < 0)  // inherited
      return 0;

    _sock = zsock_socket(addr.ss_family, SOCK_STREAM, IPPROTO_TLS_1_2);
    if (_sock < 0) return 0;

    if (!_configureTLS(host)) {
      zsock_close(_sock); _sock = -1; return 0;
    }

    _applyTimeout();  // inherited

    if (zsock_connect(_sock, (struct sockaddr*)&addr, addrlen) < 0) {
      zsock_close(_sock); _sock = -1; return 0;
    }
    return 1;
  }

  int connect(uint32_t ip, uint16_t port) override {
    char host[INET_ADDRSTRLEN];
    struct in_addr addr_in;
    addr_in.s_addr = ip;
    zsock_inet_ntop(AF_INET, &addr_in, host, sizeof(host));
    return connect(host, port);  // calls the overridden string version above
  }

 private:
  bool     _insecure;
  sec_tag_t _ca_tag, _cert_tag, _key_tag, _next_tag;

  static constexpr sec_tag_t TAG_BASE     = 100;
  static constexpr sec_tag_t TLS_TAG_NONE = 0;

  bool _addCredential(enum tls_credential_type type,
                      const uint8_t* data, size_t len, sec_tag_t* tag_out) {
    if (!data || len == 0) return false;
    sec_tag_t tag = _next_tag++;
    if (tls_credential_add(tag, type, data, len) < 0) { --_next_tag; return false; }
    *tag_out = tag;
    return true;
  }

  bool _configureTLS(const char* host) {
    int verify = _insecure ? TLS_PEER_VERIFY_NONE : TLS_PEER_VERIFY_REQUIRED;
    if (zsock_setsockopt(_sock, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify)) < 0)
      return false;

    if (host && !_insecure)
      if (zsock_setsockopt(_sock, SOL_TLS, TLS_HOSTNAME, host, strlen(host)) < 0)
        return false;

    sec_tag_t tags[3]; int n = 0;
    if (_ca_tag   != TLS_TAG_NONE) tags[n++] = _ca_tag;
    if (_cert_tag != TLS_TAG_NONE) tags[n++] = _cert_tag;
    if (_key_tag  != TLS_TAG_NONE) tags[n++] = _key_tag;

    if (n > 0)
      if (zsock_setsockopt(_sock, SOL_TLS, TLS_SEC_TAG_LIST, tags, n * sizeof(sec_tag_t)) < 0)
        return false;

    return true;
  }
};

using WiFiClientSecure = WiFiClientSecureZephyr;

}  // namespace audio_tools
