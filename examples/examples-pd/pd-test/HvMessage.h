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

#ifndef _HEAVY_MESSAGE_H_
#define _HEAVY_MESSAGE_H_

#include "HvUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ElementType {
  HV_MSG_BANG = 0,
  HV_MSG_FLOAT = 1,
  HV_MSG_SYMBOL = 2,
  HV_MSG_HASH = 3
} ElementType;

typedef struct Element {
  ElementType type;
  union {
    float f; // float
    const char *s; // symbol
    hv_uint32_t h; // hash
  } data;
} Element;

typedef struct HvMessage {
  hv_uint32_t timestamp; // the sample at which this message should be processed
  hv_uint16_t numElements;
  hv_uint16_t numBytes; // the total number of bytes that this message occupies in memory, including strings
  Element elem;
} HvMessage;

typedef struct ReceiverMessagePair {
  hv_uint32_t receiverHash;
  HvMessage msg;
} ReceiverMessagePair;

#define HV_MESSAGE_ON_STACK(_x) (HvMessage *) hv_alloca(msg_getCoreSize(_x))

/** Returns the number of bytes that this message consumes in memory, not including strings. */
static inline hv_size_t msg_getCoreSize(hv_size_t numElements) {
  hv_assert(numElements > 0);
  return sizeof(HvMessage) + ((numElements-1) * sizeof(Element));
}

HvMessage *msg_copy(const HvMessage *m);

/** Copies the message into the given buffer. The buffer must be at least as large as msg_getNumHeapBytes(). */
void msg_copyToBuffer(const HvMessage *m, char *buffer, hv_size_t len);

void msg_setElementToFrom(HvMessage *n, int indexN, const HvMessage *const m, int indexM);

/** Frees a message on the heap. Does nothing if argument is NULL. */
void msg_free(HvMessage *m);

HvMessage *msg_init(HvMessage *m, hv_size_t numElements, hv_uint32_t timestamp);

HvMessage *msg_initWithFloat(HvMessage *m, hv_uint32_t timestamp, float f);

HvMessage *msg_initWithBang(HvMessage *m, hv_uint32_t timestamp);

HvMessage *msg_initWithSymbol(HvMessage *m, hv_uint32_t timestamp, const char *s);

HvMessage *msg_initWithHash(HvMessage *m, hv_uint32_t timestamp, hv_uint32_t h);

static inline hv_uint32_t msg_getTimestamp(const HvMessage *m) {
  return m->timestamp;
}

static inline void msg_setTimestamp(HvMessage *m, hv_uint32_t timestamp) {
  m->timestamp = timestamp;
}

static inline int msg_getNumElements(const HvMessage *m) {
  return (int) m->numElements;
}

/** Returns the total number of bytes this message consumes in memory. */
static inline hv_uint32_t msg_getSize(const HvMessage *m) {
  return m->numBytes;
}

static inline ElementType msg_getType(const HvMessage *m, int index) {
  hv_assert(index < msg_getNumElements(m)); // invalid index
  return (&(m->elem)+index)->type;
}

static inline void msg_setBang(HvMessage *m, int index) {
  hv_assert(index < msg_getNumElements(m)); // invalid index
  (&(m->elem)+index)->type = HV_MSG_BANG;
  (&(m->elem)+index)->data.s = NULL;
}

static inline bool msg_isBang(const HvMessage *m, int index) {
  return (index < msg_getNumElements(m)) ? (msg_getType(m,index) == HV_MSG_BANG) : false;
}

static inline void msg_setFloat(HvMessage *m, int index, float f) {
  hv_assert(index < msg_getNumElements(m)); // invalid index
  (&(m->elem)+index)->type = HV_MSG_FLOAT;
  (&(m->elem)+index)->data.f = f;
}

static inline float msg_getFloat(const HvMessage *const m, int index) {
  hv_assert(index < msg_getNumElements(m)); // invalid index
  return (&(m->elem)+index)->data.f;
}

static inline bool msg_isFloat(const HvMessage *const m, int index) {
  return (index < msg_getNumElements(m)) ? (msg_getType(m,index) == HV_MSG_FLOAT) : false;
}

static inline void msg_setHash(HvMessage *m, int index, hv_uint32_t h) {
  hv_assert(index < msg_getNumElements(m)); // invalid index
  (&(m->elem)+index)->type = HV_MSG_HASH;
  (&(m->elem)+index)->data.h = h;
}

static inline bool msg_isHash(const HvMessage *m, int index) {
  return (index < msg_getNumElements(m)) ? (msg_getType(m, index) == HV_MSG_HASH) : false;
}

/** Returns true if the element is a hash or symbol. False otherwise. */
static inline bool msg_isHashLike(const HvMessage *m, int index) {
  return (index < msg_getNumElements(m)) ? ((msg_getType(m, index) == HV_MSG_HASH) || (msg_getType(m, index) == HV_MSG_SYMBOL)) : false;
}

/** Returns a 32-bit hash of the given element. */
hv_uint32_t msg_getHash(const HvMessage *const m, int i);

static inline void msg_setSymbol(HvMessage *m, int index, const char *s) {
  hv_assert(index < msg_getNumElements(m)); // invalid index
  hv_assert(s != NULL);
  (&(m->elem)+index)->type = HV_MSG_SYMBOL;
  (&(m->elem)+index)->data.s = s;
  // NOTE(mhroth): if the same message container is reused and string reset,
  // then the message size will be overcounted
  m->numBytes += (hv_uint16_t) (hv_strlen(s) + 1); // also count '\0'
}

static inline const char *msg_getSymbol(const HvMessage *m, int index) {
  hv_assert(index < msg_getNumElements(m)); // invalid index
  return (&(m->elem)+index)->data.s;
}

static inline bool msg_isSymbol(const HvMessage *m, int index) {
  return (index < msg_getNumElements(m)) ? (msg_getType(m, index) == HV_MSG_SYMBOL) : false;
}

bool msg_compareSymbol(const HvMessage *m, int i, const char *s);

/** Returns 1 if the element i_m of message m is equal to element i_n of message n. */
bool msg_equalsElement(const HvMessage *m, int i_m, const HvMessage *n, int i_n);

bool msg_hasFormat(const HvMessage *m, const char *fmt);

/**
 * Create a string representation of the message. Suitable for use by the print object.
 * The resulting string must be freed by the caller.
 */
char *msg_toString(const HvMessage *msg);

#ifdef __cplusplus
}
#endif

#endif // _HEAVY_MESSAGE_H_
