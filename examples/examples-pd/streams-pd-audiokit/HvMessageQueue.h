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

#ifndef _MESSAGE_QUEUE_H_
#define _MESSAGE_QUEUE_H_

#include "HvMessage.h"
#include "HvMessagePool.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
class HeavyContextInterface;
#else
typedef struct HeavyContextInterface HeavyContextInterface;
#endif

typedef struct MessageNode {
  struct MessageNode *prev; // doubly linked list
  struct MessageNode *next;
  HvMessage *m;
  void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *);
  int let;
} MessageNode;

/** A doubly linked list containing scheduled messages. */
typedef struct HvMessageQueue {
  MessageNode *head; // the head of the queue
  MessageNode *tail; // the tail of the queue
  MessageNode *pool; // the head of the reserve pool
  HvMessagePool mp;
} HvMessageQueue;

hv_size_t mq_initWithPoolSize(HvMessageQueue *q, hv_size_t poolSizeKB);

void mq_free(HvMessageQueue *q);

int mq_size(HvMessageQueue *q);

static inline HvMessage *mq_node_getMessage(MessageNode *n) {
  return n->m;
}

static inline int mq_node_getLet(MessageNode *n) {
  return n->let;
}

static inline bool mq_hasMessage(HvMessageQueue *q) {
  return (q->head != NULL);
}

// true if there is a message and it occurs before (<) timestamp
static inline bool mq_hasMessageBefore(HvMessageQueue *const q, const hv_uint32_t timestamp) {
  return mq_hasMessage(q) && (msg_getTimestamp(mq_node_getMessage(q->head)) < timestamp);
}

static inline MessageNode *mq_peek(HvMessageQueue *q) {
  return q->head;
}

/** Appends the message to the end of the queue. */
HvMessage *mq_addMessage(HvMessageQueue *q, const HvMessage *m, int let,
    void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *));

/** Insert in ascending order the message acccording to its timestamp. */
HvMessage *mq_addMessageByTimestamp(HvMessageQueue *q, const HvMessage *m, int let,
    void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *));

/** Pop the message at the head of the queue (and free its memory). */
void mq_pop(HvMessageQueue *q);

/** Remove a message from the queue (and free its memory) */
bool mq_removeMessage(HvMessageQueue *q, HvMessage *m,
    void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *));

/** Clears (and frees) all messages in the queue. */
void mq_clear(HvMessageQueue *q);

/** Removes all messages occuring at or after the given timestamp. */
void mq_clearAfter(HvMessageQueue *q, const hv_uint32_t timestamp);

#ifdef __cplusplus
}
#endif

#endif // _MESSAGE_QUEUE_H_
