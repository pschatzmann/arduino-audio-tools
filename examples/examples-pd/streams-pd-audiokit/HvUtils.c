/**
 * Copyright (c) 2014-2018 Enzien Audio Ltd.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "HvUtils.h"

hv_uint32_t hv_string_to_hash(const char *str) {
  // this hash is based MurmurHash2
  // http://en.wikipedia.org/wiki/MurmurHash
  // https://sites.google.com/site/murmurhash/
  static const hv_uint32_t n = 0x5bd1e995;
  static const hv_int32_t r = 24;

  if (str == NULL) return 0;

  hv_uint32_t len = (hv_uint32_t) hv_strlen(str);
  hv_uint32_t x = len; // seed (0) ^ len

  while (len >= 4) {
#if HV_EMSCRIPTEN
    hv_uint32_t k = str[0] | (str[1] << 8) | (str[2] << 16) | (str[3] << 24);
#else
    hv_uint32_t k = *((hv_uint32_t *) str);
#endif
    k *= n;
    k ^= (k >> r);
    k *= n;
    x *= n;
    x ^= k;
    str += 4; len -= 4;
  }
  switch (len) {
    case 3: x ^= (str[2] << 16);
    case 2: x ^= (str[1] << 8);
    case 1: x ^= str[0]; x *= n;
    default: break;
  }
  x ^= (x >> 13);
  x *= n;
  x ^= (x >> 15);
  return x;
}
