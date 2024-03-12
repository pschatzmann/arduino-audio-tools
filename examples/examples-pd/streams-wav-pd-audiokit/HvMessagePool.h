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

#ifndef _MESSAGE_POOL_H_
#define _MESSAGE_POOL_H_

#include "HvUtils.h"

#ifdef HV_MP_NUM_MESSAGE_LISTS
#define MP_NUM_MESSAGE_LISTS HV_MP_NUM_MESSAGE_LISTS
#else // HV_MP_NUM_MESSAGE_LISTS
#define MP_NUM_MESSAGE_LISTS 4
#endif // HV_MP_NUM_MESSAGE_LISTS

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HvMessagePoolList {
  struct MessageListNode *head; // list of currently available blocks
  struct MessageListNode *pool; // list of currently used blocks
} HvMessagePoolList;

typedef struct HvMessagePool {
  char *buffer; // the buffer of all messages
  hv_size_t bufferSize; // in bytes
  hv_size_t bufferIndex; // the number of total reserved bytes

  HvMessagePoolList lists[MP_NUM_MESSAGE_LISTS];
} HvMessagePool;

/**
 * The HvMessagePool is a basic memory management system. It reserves a large block of memory at initialisation
 * and proceeds to divide this block into smaller chunks (usually 512 bytes) as they are needed. These chunks are
 * further divided into 32, 64, 128, or 256 sections. Each of these sections is managed by a HvMessagePoolList (MPL).
 * An MPL is a linked-list data structure which is initialised such that its own pool of listnodes is filled with nodes
 * that point at each subblock (e.g. each 32-byte block of a 512-block chunk).
 *
 * HvMessagePool is loosely inspired by TCMalloc. http://goog-perftools.sourceforge.net/doc/tcmalloc.html
 */

hv_size_t mp_init(struct HvMessagePool *mp, hv_size_t numKB);

void mp_free(struct HvMessagePool *mp);

/**
 * Adds a message to the pool and returns a pointer to the copy. Returns NULL
 * if no space was available in the pool.
 */
struct HvMessage *mp_addMessage(struct HvMessagePool *mp, const struct HvMessage *m);

void mp_freeMessage(struct HvMessagePool *mp, struct HvMessage *m);

#ifdef __cplusplus
}
#endif

#endif // _MESSAGE_POOL_H_
