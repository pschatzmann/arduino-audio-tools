#pragma once


#ifndef htons
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#  define htonl(x) (((x) << 24 & 0xFF000000UL) | ((x) << 8 & 0x00FF0000UL) | ((x) >> 8 & 0x0000FF00UL) | ((x) >> 24 & 0x000000FFUL))
#  define htons(x) (((uint16_t)(x)&0xff00) >> 8) | (((uint16_t)(x)&0X00FF) << 8)
#  define ntohl(x) htonl(x)
#  define ntohs(x) htons(x)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define htonl(x) x
#  define htons(x) x
#  define ntohl(x) x
#  define ntohs(x) x
#else
#error Could not determine byte order
#endif
#endif

/// support for 64 bytes
#ifndef htonll
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#  define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#  define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define htonll(x) x
#  define ntohll(x) x
#endif
#endif
