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

#include "HvMessage.h"

HvMessage *msg_init(HvMessage *m, hv_size_t numElements, hv_uint32_t timestamp) {
  m->timestamp = timestamp;
  m->numElements = (hv_uint16_t) numElements;
  m->numBytes = (hv_uint16_t) msg_getCoreSize(numElements);
  return m;
}

HvMessage *msg_initWithFloat(HvMessage *m, hv_uint32_t timestamp, float f) {
  m->timestamp = timestamp;
  m->numElements = 1;
  m->numBytes = sizeof(HvMessage);
  msg_setFloat(m, 0, f);
  return m;
}

HvMessage *msg_initWithBang(HvMessage *m, hv_uint32_t timestamp) {
  m->timestamp = timestamp;
  m->numElements = 1;
  m->numBytes = sizeof(HvMessage);
  msg_setBang(m, 0);
  return m;
}

HvMessage *msg_initWithSymbol(HvMessage *m, hv_uint32_t timestamp, const char *s) {
  m->timestamp = timestamp;
  m->numElements = 1;
  m->numBytes = sizeof(HvMessage) + (hv_uint16_t) hv_strlen(s);
  msg_setSymbol(m, 0, s);
  return m;
}

HvMessage *msg_initWithHash(HvMessage *m, hv_uint32_t timestamp, hv_uint32_t h) {
  m->timestamp = timestamp;
  m->numElements = 1;
  m->numBytes = sizeof(HvMessage);
  msg_setHash(m, 0, h);
  return m;
}

void msg_copyToBuffer(const HvMessage *m, char *buffer, hv_size_t len) {
  HvMessage *r = (HvMessage *) buffer;

  hv_size_t len_r = msg_getCoreSize(msg_getNumElements(m));

  // assert that the message is not already larger than the length of the buffer
  hv_assert(len_r <= len);

  // copy the basic message to the buffer
  hv_memcpy(r, m, len_r);

  char *p = buffer + len_r; // points to the end of the base message
  for (int i = 0; i < msg_getNumElements(m); ++i) {
    if (msg_isSymbol(m,i)) {
      const hv_size_t symLen = (hv_size_t) hv_strlen(msg_getSymbol(m,i)) + 1; // include the trailing null char
      hv_assert(len_r + symLen <= len); // stay safe!
      hv_strncpy(p, msg_getSymbol(m,i), symLen);
      msg_setSymbol(r, i, p);
      p += symLen;
      len_r += symLen;
    }
  }

  r->numBytes = (hv_uint16_t) len_r; // update the message size in memory
}

// the message is serialised such that all symbol elements are placed in order at the end of the buffer
HvMessage *msg_copy(const HvMessage *m) {
  const hv_uint32_t heapSize = msg_getSize(m);
  char *r = (char *) hv_malloc(heapSize);
  hv_assert(r != NULL);
  msg_copyToBuffer(m, r, heapSize);
  return (HvMessage *) r;
}

void msg_free(HvMessage *m) {
  hv_free(m); // because heap messages are serialised in memory, a simple call to free releases the message
}

bool msg_hasFormat(const HvMessage *m, const char *fmt) {
  hv_assert(fmt != NULL);
  const int n = msg_getNumElements(m);
  for (int i = 0; i < n; ++i) {
    switch (fmt[i]) {
      case 'b': if (!msg_isBang(m, i)) return false; break;
      case 'f': if (!msg_isFloat(m, i)) return false; break;
      case 'h': if (!msg_isHash(m, i)) return false; break;
      case 's': if (!msg_isSymbol(m, i)) return false; break;
      default: return false;
    }
  }
  return (fmt[n] == '\0');
}

bool msg_compareSymbol(const HvMessage *m, int i, const char *s) {
  switch (msg_getType(m,i)) {
    case HV_MSG_SYMBOL: return !hv_strcmp(msg_getSymbol(m, i), s);
    case HV_MSG_HASH: return (msg_getHash(m,i) == hv_string_to_hash(s));
    default: return false;
  }
}

bool msg_equalsElement(const HvMessage *m, int i_m, const HvMessage *n, int i_n) {
  if (i_m < msg_getNumElements(m) && i_n < msg_getNumElements(n)) {
    if (msg_getType(m, i_m) == msg_getType(n, i_n)) {
      switch (msg_getType(m, i_m)) {
        case HV_MSG_BANG: return true;
        case HV_MSG_FLOAT: return (msg_getFloat(m, i_m) == msg_getFloat(n, i_n));
        case HV_MSG_SYMBOL: return msg_compareSymbol(m, i_m, msg_getSymbol(n, i_n));
        case HV_MSG_HASH: return msg_getHash(m,i_m) == msg_getHash(n,i_n);
        default: break;
      }
    }
  }
  return false;
}

void msg_setElementToFrom(HvMessage *n, int i_n, const HvMessage *const m, int i_m) {
  switch (msg_getType(m, i_m)) {
    case HV_MSG_BANG: msg_setBang(n, i_n); break;
    case HV_MSG_FLOAT: msg_setFloat(n, i_n, msg_getFloat(m, i_m)); break;
    case HV_MSG_SYMBOL: msg_setSymbol(n, i_n, msg_getSymbol(m, i_m)); break;
    case HV_MSG_HASH: msg_setHash(n, i_n, msg_getHash(m, i_m));
    default: break;
  }
}

hv_uint32_t msg_getHash(const HvMessage *const m, int i) {
  hv_assert(i < msg_getNumElements(m)); // invalid index
  switch (msg_getType(m,i)) {
    case HV_MSG_BANG: return 0xFFFFFFFF;
    case HV_MSG_FLOAT: {
      float f = msg_getFloat(m,i);
      return *((hv_uint32_t *) &f);
    }
    case HV_MSG_SYMBOL: return hv_string_to_hash(msg_getSymbol(m,i));
    case HV_MSG_HASH: return (&(m->elem)+i)->data.h;
    default: return 0;
  }
}

char *msg_toString(const HvMessage *m) {
  hv_assert(msg_getNumElements(m) > 0);
  int *len = (int *) hv_alloca(msg_getNumElements(m)*sizeof(int));
  int size = 0; // the total length of our final buffer

  // loop through every element in our list of atoms
  // first loop figures out how long our buffer should be
  for (int i = 0; i < msg_getNumElements(m); i++) {
    // length of our string is each atom plus a space, or \0 on the end
    switch (msg_getType(m, i)) {
      case HV_MSG_BANG: len[i] = hv_snprintf(NULL, 0, "%s", "bang") + 1; break;
      case HV_MSG_FLOAT: len[i] = hv_snprintf(NULL, 0, "%g", msg_getFloat(m, i)) + 1; break;
      case HV_MSG_SYMBOL: len[i] = hv_snprintf(NULL, 0, "%s", msg_getSymbol(m, i)) + 1; break;
      case HV_MSG_HASH: len[i] = hv_snprintf(NULL, 0, "0x%X", msg_getHash(m, i)) + 1; break;
      default: break;
    }
    size += len[i];
  }

  hv_assert(size > 0);

  // now we do the piecewise concatenation into our final string
  // the final buffer we will pass back after concatenating all strings - user should free it
  char *finalString = (char *) hv_malloc(size*sizeof(char));
  hv_assert(finalString != NULL);
  int pos = 0;
  for (int i = 0; i < msg_getNumElements(m); i++) {
    // put a string representation of each atom into the final string
    switch (msg_getType(m, i)) {
      case HV_MSG_BANG: hv_snprintf(finalString+pos, len[i], "%s", "bang"); break;
      case HV_MSG_FLOAT: hv_snprintf(finalString+pos, len[i], "%g", msg_getFloat(m, i)); break;
      case HV_MSG_SYMBOL: hv_snprintf(finalString+pos, len[i], "%s", msg_getSymbol(m, i)); break;
      case HV_MSG_HASH: hv_snprintf(finalString+pos, len[i], "0x%X", msg_getHash(m, i)); break;
      default: break;
    }
    pos += len[i];
    finalString[pos-1] = 32; // ASCII space
  }
  finalString[size-1] = '\0'; // ensure that the string is null terminated
  return finalString;
}
