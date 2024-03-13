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

#include "HeavyContext.hpp"
#include "HvTable.h"

void defaultSendHook(HeavyContextInterface *context,
    const char *sendName, hv_uint32_t sendHash, const HvMessage *msg) {
  HeavyContext *thisContext = reinterpret_cast<HeavyContext *>(context);
  const hv_uint32_t numBytes = sizeof(ReceiverMessagePair) + msg_getSize(msg) - sizeof(HvMessage);
  ReceiverMessagePair *p = reinterpret_cast<ReceiverMessagePair *>(hLp_getWriteBuffer(&thisContext->outQueue, numBytes));
  if (p != nullptr) {
    p->receiverHash = sendHash;
    msg_copyToBuffer(msg, (char *) &p->msg, msg_getSize(msg));
    hLp_produce(&thisContext->outQueue, numBytes);
  } else {
    hv_assert(false &&
        "::defaultSendHook - The out message queue is full and cannot accept more messages until they "
        "have been processed. Try increasing the outQueueKb size in the new_with_options() constructor.");
  }
}

HeavyContext::HeavyContext(double sampleRate, int poolKb, int inQueueKb, int outQueueKb) :
    sampleRate(sampleRate) {

  hv_assert(sampleRate > 0.0); // sample rate must be positive
  hv_assert(poolKb > 0);
  hv_assert(inQueueKb > 0);
  hv_assert(outQueueKb >= 0);

  blockStartTimestamp = 0;
  printHook = nullptr;
  userData = nullptr;

  // if outQueueKb is positive, then the outQueue is allocated and the default sendhook is set.
  // Otherwise outQueue and the sendhook are set to NULL.
  sendHook = (outQueueKb > 0) ? &defaultSendHook : nullptr;

  HV_SPINLOCK_RELEASE(inQueueLock);
  HV_SPINLOCK_RELEASE(outQueueLock);

  numBytes = sizeof(HeavyContext);

  numBytes += mq_initWithPoolSize(&mq, poolKb);
  numBytes += hLp_init(&inQueue, inQueueKb * 1024);
  numBytes += hLp_init(&outQueue, outQueueKb * 1024); // outQueueKb value of 0 sets everything to NULL
}

HeavyContext::~HeavyContext() {
  mq_free(&mq);
  hLp_free(&inQueue);
  hLp_free(&outQueue);
}

bool HeavyContext::sendBangToReceiver(hv_uint32_t receiverHash) {
  HvMessage *m = HV_MESSAGE_ON_STACK(1);
  msg_initWithBang(m, 0);
  bool success = sendMessageToReceiver(receiverHash, 0.0, m);
  return success;
}

bool HeavyContext::sendFloatToReceiver(hv_uint32_t receiverHash, float f) {
  HvMessage *m = HV_MESSAGE_ON_STACK(1);
  msg_initWithFloat(m, 0, f);
  bool success = sendMessageToReceiver(receiverHash, 0.0, m);
  return success;
}

bool HeavyContext::sendSymbolToReceiver(hv_uint32_t receiverHash, const char *s) {
  hv_assert(s != nullptr);
  HvMessage *m = HV_MESSAGE_ON_STACK(1);
  msg_initWithSymbol(m, 0, (char *) s);
  bool success = sendMessageToReceiver(receiverHash, 0.0, m);
  return success;
}

bool HeavyContext::sendMessageToReceiverV(hv_uint32_t receiverHash, double delayMs, const char *format, ...) {
  hv_assert(delayMs >= 0.0);
  hv_assert(format != nullptr);

  va_list ap;
  va_start(ap, format);
  const int numElem = (int) hv_strlen(format);
  HvMessage *m = HV_MESSAGE_ON_STACK(numElem);
  msg_init(m, numElem, blockStartTimestamp + (hv_uint32_t) (hv_max_d(0.0, delayMs)*getSampleRate()/1000.0));
  for (int i = 0; i < numElem; i++) {
    switch (format[i]) {
      case 'b': msg_setBang(m, i); break;
      case 'f': msg_setFloat(m, i, (float) va_arg(ap, double)); break;
      case 'h': msg_setHash(m, i, (int) va_arg(ap, int)); break;
      case 's': msg_setSymbol(m, i, (char *) va_arg(ap, char *)); break;
      default: break;
    }
  }
  va_end(ap);

  bool success = sendMessageToReceiver(receiverHash, delayMs, m);
  return success;
}

bool HeavyContext::sendMessageToReceiver(hv_uint32_t receiverHash, double delayMs, HvMessage *m) {
  hv_assert(delayMs >= 0.0);
  hv_assert(m != nullptr);

  const hv_uint32_t timestamp = blockStartTimestamp +
      (hv_uint32_t) (hv_max_d(0.0, delayMs)*(getSampleRate()/1000.0));

  ReceiverMessagePair *p = nullptr;
  HV_SPINLOCK_ACQUIRE(inQueueLock);
  const hv_uint32_t numBytes = sizeof(ReceiverMessagePair) + msg_getSize(m) - sizeof(HvMessage);
  p = (ReceiverMessagePair *) hLp_getWriteBuffer(&inQueue, numBytes);
  if (p != nullptr) {
    p->receiverHash = receiverHash;
    msg_copyToBuffer(m, (char *) &p->msg, msg_getSize(m));
    msg_setTimestamp(&p->msg, timestamp);
    hLp_produce(&inQueue, numBytes);
  } else {
    hv_assert(false &&
        "::sendMessageToReceiver - The input message queue is full and cannot accept more messages until they "
        "have been processed. Try increasing the inQueueKb size in the new_with_options() constructor.");
  }
  HV_SPINLOCK_RELEASE(inQueueLock);
  return (p != nullptr);
}

bool HeavyContext::cancelMessage(HvMessage *m, void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *)) {
  return mq_removeMessage(&mq, m, sendMessage);
}

HvMessage *HeavyContext::scheduleMessageForObject(const HvMessage *m,
    void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *),
    int letIndex) {
  HvMessage *n = mq_addMessageByTimestamp(&mq, m, letIndex, sendMessage);
  return n;
}

float *HeavyContext::getBufferForTable(hv_uint32_t tableHash) {
  HvTable *t = getTableForHash(tableHash);
  if (t != nullptr) {
    return hTable_getBuffer(t);
  } else return nullptr;
}

int HeavyContext::getLengthForTable(hv_uint32_t tableHash) {
  HvTable *t = getTableForHash(tableHash);
  if (t != nullptr) {
    return hTable_getLength(t);
  } else return 0;
}

bool HeavyContext::setLengthForTable(hv_uint32_t tableHash, hv_uint32_t newSampleLength) {
  HvTable *t = getTableForHash(tableHash);
  if (t != nullptr) {
    hTable_resize(t, newSampleLength);
    return true;
  } else return false;
}

void HeavyContext::lockAcquire() {
  HV_SPINLOCK_ACQUIRE(inQueueLock);
}

bool HeavyContext::lockTry() {
  HV_SPINLOCK_TRY(inQueueLock);
}

void HeavyContext::lockRelease() {
  HV_SPINLOCK_RELEASE(inQueueLock);
}

void HeavyContext::setInputMessageQueueSize(int inQueueKb) {
  hv_assert(inQueueKb > 0);
  hLp_free(&inQueue);
  hLp_init(&inQueue, inQueueKb*1024);
}

void HeavyContext::setOutputMessageQueueSize(int outQueueKb) {
  hv_assert(outQueueKb > 0);
  hLp_free(&outQueue);
  hLp_init(&outQueue, outQueueKb*1024);
}

bool HeavyContext::getNextSentMessage(hv_uint32_t *destinationHash, HvMessage *outMsg, hv_size_t msgLengthBytes) {
  *destinationHash = 0;
  ReceiverMessagePair *p = nullptr;
  hv_assert((sendHook == &defaultSendHook) &&
      "::getNextSentMessage - this function won't do anything if the msg outQueue "
      "size is 0, or you've overriden the default sendhook.");
  if (sendHook == &defaultSendHook) {
    HV_SPINLOCK_ACQUIRE(outQueueLock);
    if (hLp_hasData(&outQueue)) {
      hv_uint32_t numBytes = 0;
      p = reinterpret_cast<ReceiverMessagePair *>(hLp_getReadBuffer(&outQueue, &numBytes));
      hv_assert((p != nullptr) && "::getNextSentMessage - something bad happened.");
      hv_assert(numBytes >= sizeof(ReceiverMessagePair));
      hv_assert((numBytes <= msgLengthBytes) &&
          "::getNextSentMessage - the sent message is bigger than the message "
          "passed to handle it.");
      *destinationHash = p->receiverHash;
      hv_memcpy(outMsg, &p->msg, numBytes);
      hLp_consume(&outQueue);
    }
    HV_SPINLOCK_RELEASE(outQueueLock);
  }
  return (p != nullptr);
}

hv_uint32_t HeavyContext::getHashForString(const char *str) {
  return hv_string_to_hash(str);
}

HvTable *_hv_table_get(HeavyContextInterface *c, hv_uint32_t tableHash) {
  hv_assert(c != nullptr);
  return reinterpret_cast<HeavyContext *>(c)->getTableForHash(tableHash);
}

void _hv_scheduleMessageForReceiver(HeavyContextInterface *c, hv_uint32_t receiverHash, HvMessage *m) {
  hv_assert(c != nullptr);
  reinterpret_cast<HeavyContext *>(c)->scheduleMessageForReceiver(receiverHash, m);
}

HvMessage *_hv_scheduleMessageForObject(HeavyContextInterface *c, const HvMessage *m,
    void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *),
    int letIndex) {
  hv_assert(c != nullptr);
  HvMessage *n = reinterpret_cast<HeavyContext *>(c)->scheduleMessageForObject(
      m, sendMessage, letIndex);
  return n;
}

#ifdef __cplusplus
extern "C" {
#endif

HvTable *hv_table_get(HeavyContextInterface *c, hv_uint32_t tableHash) {
  return _hv_table_get(c, tableHash);
}

void hv_scheduleMessageForReceiver(HeavyContextInterface *c, hv_uint32_t receiverHash, HvMessage *m) {
  _hv_scheduleMessageForReceiver(c, receiverHash, m);
}

HvMessage *hv_scheduleMessageForObject(HeavyContextInterface *c, const HvMessage *m,
    void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *),
    int letIndex) {
  return _hv_scheduleMessageForObject(c, m, sendMessage, letIndex);
}

#ifdef __cplusplus
}
#endif
