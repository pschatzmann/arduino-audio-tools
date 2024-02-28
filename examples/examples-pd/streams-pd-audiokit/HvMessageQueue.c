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

#include "HvMessageQueue.h"

hv_size_t mq_initWithPoolSize(HvMessageQueue *q, hv_size_t poolSizeKB) {
  hv_assert(poolSizeKB > 0);
  q->head = NULL;
  q->tail = NULL;
  q->pool = NULL;
  return mp_init(&q->mp, poolSizeKB);
}

void mq_free(HvMessageQueue *q) {
  mq_clear(q);
  while (q->pool != NULL) {
    MessageNode *n = q->pool;
    q->pool = q->pool->next;
    hv_free(n);
  }
  mp_free(&q->mp);
}

static MessageNode *mq_getOrCreateNodeFromPool(HvMessageQueue *q) {
  if (q->pool == NULL) {
    // if necessary, create a new empty node
    q->pool = (MessageNode *) hv_malloc(sizeof(MessageNode));
    hv_assert(q->pool != NULL);
    q->pool->next = NULL;
  }
  MessageNode *node = q->pool;
  q->pool = q->pool->next;
  return node;
}

int mq_size(HvMessageQueue *q) {
  int size = 0;
  MessageNode *n = q->head;
  while (n != NULL) {
    ++size;
    n = n->next;
  }
  return size;
}

HvMessage *mq_addMessage(HvMessageQueue *q, const HvMessage *m, int let,
    void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *)) {
  MessageNode *node = mq_getOrCreateNodeFromPool(q);
  node->m = mp_addMessage(&q->mp, m);
  node->let = let;
  node->sendMessage = sendMessage;
  node->prev = NULL;
  node->next = NULL;

  if (q->tail != NULL) {
    // the list already contains elements
    q->tail->next = node;
    node->prev = q->tail;
    q->tail = node;
  } else {
    // the list is empty
    node->prev = NULL;
    q->head = node;
    q->tail = node;
  }
  return mq_node_getMessage(node);
}

HvMessage *mq_addMessageByTimestamp(HvMessageQueue *q, const HvMessage *m, int let,
    void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *)) {
  if (mq_hasMessage(q)) {
    MessageNode *n = mq_getOrCreateNodeFromPool(q);
    n->m = mp_addMessage(&q->mp, m);
    n->let = let;
    n->sendMessage = sendMessage;

    if (msg_getTimestamp(m) < msg_getTimestamp(q->head->m)) {
      // the message occurs before the current head
      n->next = q->head;
      q->head->prev = n;
      n->prev = NULL;
      q->head = n;
    } else if (msg_getTimestamp(m) >= msg_getTimestamp(q->tail->m)) {
      // the message occurs after the current tail
      n->next = NULL;
      n->prev = q->tail;
      q->tail->next = n;
      q->tail = n;
    } else {
      // the message occurs somewhere between the head and tail
      MessageNode *node = q->head;
      while (node != NULL) {
        if (msg_getTimestamp(m) < msg_getTimestamp(node->next->m)) {
          MessageNode *r = node->next;
          node->next = n;
          n->next = r;
          n->prev = node;
          r->prev = n;
          break;
        }
        node = node->next;
      }
    }
    return n->m;
  } else {
    // add a message to the head
    return mq_addMessage(q, m, let, sendMessage);
  }
}

void mq_pop(HvMessageQueue *q) {
  if (mq_hasMessage(q)) {
    MessageNode *n = q->head;

    mp_freeMessage(&q->mp, n->m);
    n->m = NULL;

    n->let = 0;
    n->sendMessage = NULL;

    q->head = n->next;
    if (q->head == NULL) {
      q->tail = NULL;
    } else {
      q->head->prev = NULL;
    }
    n->next = q->pool;
    n->prev = NULL;
    q->pool = n;
  }
}

bool mq_removeMessage(HvMessageQueue *q, HvMessage *m, void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *)) {
  if (mq_hasMessage(q)) {
    if (mq_node_getMessage(q->head) == m) { // msg in head node
      // only remove the message if sendMessage is the same as the stored one,
      // if the sendMessage argument is NULL, it is not checked and will remove any matching message pointer
      if (sendMessage == NULL || q->head->sendMessage == sendMessage) {
        mq_pop(q);
        return true;
      }
    } else {
      MessageNode *prevNode = q->head;
      MessageNode *currNode = q->head->next;
      while ((currNode != NULL) && (currNode->m != m)) {
        prevNode = currNode;
        currNode = currNode->next;
      }
      if (currNode != NULL) {
        if (sendMessage == NULL || currNode->sendMessage == sendMessage) {
          mp_freeMessage(&q->mp, m);
          currNode->m = NULL;
          currNode->let = 0;
          currNode->sendMessage = NULL;
          if (currNode == q->tail) { // msg in tail node
            prevNode->next = NULL;
            q->tail = prevNode;
          } else { // msg in middle node
            prevNode->next = currNode->next;
            currNode->next->prev = prevNode;
          }
          currNode->next = (q->pool == NULL) ? NULL : q->pool;
          currNode->prev = NULL;
          q->pool = currNode;
          return true;
        }
      }
    }
  }
  return false;
}

void mq_clear(HvMessageQueue *q) {
  while (mq_hasMessage(q)) {
    mq_pop(q);
  }
}

void mq_clearAfter(HvMessageQueue *q, const hv_uint32_t timestamp) {
  MessageNode *n = q->tail;
  while (n != NULL && timestamp <= msg_getTimestamp(n->m)) {
    // free the node's message
    mp_freeMessage(&q->mp, n->m);
    n->m = NULL;
    n->let = 0;
    n->sendMessage = NULL;

    // the tail points at the previous node
    q->tail = n->prev;

    // put the node back in the pool
    n->next = q->pool;
    n->prev = NULL;
    if (q->pool != NULL) q->pool->prev = n;
    q->pool = n;

    // update the tail node
    n = q->tail;
  }

  if (q->tail == NULL) q->head = NULL;
}
