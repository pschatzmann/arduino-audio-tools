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

#ifdef __cplusplus
extern "C" {
#endif

#if HV_APPLE
#pragma mark - Heavy Table
#endif

HV_EXPORT bool hv_table_setLength(HeavyContextInterface *c, hv_uint32_t tableHash, hv_uint32_t newSampleLength) {
  hv_assert(c != nullptr);
  return c->setLengthForTable(tableHash, newSampleLength);
}

HV_EXPORT float *hv_table_getBuffer(HeavyContextInterface *c, hv_uint32_t tableHash) {
  hv_assert(c != nullptr);
  return c->getBufferForTable(tableHash);
}

HV_EXPORT hv_uint32_t hv_table_getLength(HeavyContextInterface *c, hv_uint32_t tableHash) {
  hv_assert(c != nullptr);
  return c->getLengthForTable(tableHash);
}



#if HV_APPLE
#pragma mark - Heavy Message
#endif

HV_EXPORT hv_size_t hv_msg_getByteSize(hv_uint32_t numElements) {
  return msg_getCoreSize(numElements);
}

HV_EXPORT void hv_msg_init(HvMessage *m, int numElements, hv_uint32_t timestamp) {
  msg_init(m, numElements, timestamp);
}

HV_EXPORT hv_size_t hv_msg_getNumElements(const HvMessage *m) {
  return msg_getNumElements(m);
}

HV_EXPORT hv_uint32_t hv_msg_getTimestamp(const HvMessage *m) {
  return msg_getTimestamp(m);
}

HV_EXPORT void hv_msg_setTimestamp(HvMessage *m, hv_uint32_t timestamp) {
  msg_setTimestamp(m, timestamp);
}

HV_EXPORT bool hv_msg_isBang(const HvMessage *const m, int i) {
  return msg_isBang(m,i);
}

HV_EXPORT void hv_msg_setBang(HvMessage *m, int i) {
  msg_setBang(m,i);
}

HV_EXPORT bool hv_msg_isFloat(const HvMessage *const m, int i) {
  return msg_isFloat(m, i);
}

HV_EXPORT float hv_msg_getFloat(const HvMessage *const m, int i) {
  return msg_getFloat(m,i);
}

HV_EXPORT void hv_msg_setFloat(HvMessage *m, int i, float f) {
  msg_setFloat(m,i,f);
}

HV_EXPORT bool hv_msg_isSymbol(const HvMessage *const m, int i) {
  return msg_isSymbol(m,i);
}

HV_EXPORT const char *hv_msg_getSymbol(const HvMessage *const m, int i) {
  return msg_getSymbol(m,i);
}

HV_EXPORT void hv_msg_setSymbol(HvMessage *m, int i, const char *s) {
  msg_setSymbol(m,i,s);
}

HV_EXPORT bool hv_msg_isHash(const HvMessage *const m, int i) {
  return msg_isHash(m, i);
}

HV_EXPORT hv_uint32_t hv_msg_getHash(const HvMessage *const m, int i) {
  return msg_getHash(m, i);
}

HV_EXPORT bool hv_msg_hasFormat(const HvMessage *const m, const char *fmt) {
  return msg_hasFormat(m, fmt);
}

HV_EXPORT char *hv_msg_toString(const HvMessage *const m) {
  return msg_toString(m);
}

HV_EXPORT HvMessage *hv_msg_copy(const HvMessage *const m) {
  return msg_copy(m);
}

HV_EXPORT void hv_msg_free(HvMessage *m) {
  msg_free(m);
}



#if HV_APPLE
#pragma mark - Heavy Common
#endif

HV_EXPORT int hv_getSize(HeavyContextInterface *c) {
  hv_assert(c != nullptr);
  return (int) c->getSize();
}

HV_EXPORT double hv_getSampleRate(HeavyContextInterface *c) {
  hv_assert(c != nullptr);
  return c->getSampleRate();
}

HV_EXPORT int hv_getNumInputChannels(HeavyContextInterface *c) {
  hv_assert(c != nullptr);
  return c->getNumInputChannels();
}

HV_EXPORT int hv_getNumOutputChannels(HeavyContextInterface *c) {
  hv_assert(c != nullptr);
  return c->getNumOutputChannels();
}

HV_EXPORT void hv_setPrintHook(HeavyContextInterface *c, HvPrintHook_t *f) {
  hv_assert(c != nullptr);
  c->setPrintHook(f);
}

HV_EXPORT HvPrintHook_t *hv_getPrintHook(HeavyContextInterface *c) {
  hv_assert(c != nullptr);
  return c->getPrintHook();
}

HV_EXPORT void hv_setSendHook(HeavyContextInterface *c, HvSendHook_t *f) {
  hv_assert(c != nullptr);
  c->setSendHook(f);
}

HV_EXPORT hv_uint32_t hv_stringToHash(const char *s) {
  return hv_string_to_hash(s);
}

HV_EXPORT bool hv_sendBangToReceiver(HeavyContextInterface *c, hv_uint32_t receiverHash) {
  hv_assert(c != nullptr);
  return c->sendBangToReceiver(receiverHash);
}

HV_EXPORT bool hv_sendFloatToReceiver(HeavyContextInterface *c, hv_uint32_t receiverHash, float x) {
  hv_assert(c != nullptr);
  return c->sendFloatToReceiver(receiverHash, x);
}

HV_EXPORT bool hv_sendSymbolToReceiver(HeavyContextInterface *c, hv_uint32_t receiverHash, char *s) {
  hv_assert(c != nullptr);
  return c->sendSymbolToReceiver(receiverHash, s);
}

HV_EXPORT bool hv_sendMessageToReceiverV(
    HeavyContextInterface *c, hv_uint32_t receiverHash, double delayMs, const char *format, ...) {
  hv_assert(c != nullptr);
  hv_assert(delayMs >= 0.0);
  hv_assert(format != nullptr);

  va_list ap;
  va_start(ap, format);
  const int numElem = (int) hv_strlen(format);
  HvMessage *m = HV_MESSAGE_ON_STACK(numElem);
  msg_init(m, numElem, c->getCurrentSample() + (hv_uint32_t) (hv_max_d(0.0, delayMs)*c->getSampleRate()/1000.0));
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

  return c->sendMessageToReceiver(receiverHash, delayMs, m);
}

HV_EXPORT bool hv_sendMessageToReceiver(
    HeavyContextInterface *c, hv_uint32_t receiverHash, double delayMs, HvMessage *m) {
  hv_assert(c != nullptr);
  return c->sendMessageToReceiver(receiverHash, delayMs, m);
}

HV_EXPORT void hv_cancelMessage(HeavyContextInterface *c, HvMessage *m, void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *)) {
  hv_assert(c != nullptr);
  c->cancelMessage(m, sendMessage);
}

HV_EXPORT const char *hv_getName(HeavyContextInterface *c) {
  hv_assert(c != nullptr);
  return c->getName();
}

HV_EXPORT void hv_setUserData(HeavyContextInterface *c, void *userData) {
  hv_assert(c != nullptr);
  c->setUserData(userData);
}

HV_EXPORT void *hv_getUserData(HeavyContextInterface *c) {
  hv_assert(c != nullptr);
  return c->getUserData();
}

HV_EXPORT double hv_getCurrentTime(HeavyContextInterface *c) {
  hv_assert(c != nullptr);
  return (double) c->samplesToMilliseconds(c->getCurrentSample());
}

HV_EXPORT hv_uint32_t hv_getCurrentSample(HeavyContextInterface *c) {
  hv_assert(c != nullptr);
  return c->getCurrentSample();
}

HV_EXPORT float hv_samplesToMilliseconds(HeavyContextInterface *c, hv_uint32_t numSamples) {
  hv_assert(c != nullptr);
  return c->samplesToMilliseconds(numSamples);
}

HV_EXPORT hv_uint32_t hv_millisecondsToSamples(HeavyContextInterface *c, float ms) {
  hv_assert(c != nullptr);
  return c->millisecondsToSamples(ms);
}

HV_EXPORT int hv_getParameterInfo(HeavyContextInterface *c, int index, HvParameterInfo *info) {
  hv_assert(c != nullptr);
  return c->getParameterInfo(index, info);
}

HV_EXPORT void hv_lock_acquire(HeavyContextInterface *c) {
  hv_assert(c != nullptr);
  c->lockAcquire();
}

HV_EXPORT bool hv_lock_try(HeavyContextInterface *c) {
  hv_assert(c != nullptr);
  return c->lockTry();
}

HV_EXPORT void hv_lock_release(HeavyContextInterface *c) {
  hv_assert(c != nullptr);
  c->lockRelease();
}

HV_EXPORT void hv_setInputMessageQueueSize(HeavyContextInterface *c, hv_uint32_t inQueueKb) {
  hv_assert(c != nullptr);
  c->setInputMessageQueueSize(inQueueKb);
}

HV_EXPORT void hv_setOutputMessageQueueSize(HeavyContextInterface *c, hv_uint32_t outQueueKb) {
  hv_assert(c != nullptr);
  c->setOutputMessageQueueSize(outQueueKb);
}

HV_EXPORT bool hv_getNextSentMessage(HeavyContextInterface *c, hv_uint32_t *destinationHash, HvMessage *outMsg, hv_uint32_t msgLength) {
  hv_assert(c != nullptr);
  hv_assert(destinationHash != nullptr);
  hv_assert(outMsg != nullptr);
  return c->getNextSentMessage(destinationHash, outMsg, msgLength);
}


#if HV_APPLE
#pragma mark - Heavy Common
#endif

HV_EXPORT int hv_process(HeavyContextInterface *c, float **inputBuffers, float **outputBuffers, int n) {
  hv_assert(c != nullptr);
  return c->process(inputBuffers, outputBuffers, n);
}

HV_EXPORT int hv_processInline(HeavyContextInterface *c, float *inputBuffers, float *outputBuffers, int n) {
  hv_assert(c != nullptr);
  return c->processInline(inputBuffers, outputBuffers, n);
}

HV_EXPORT int hv_processInlineInterleaved(HeavyContextInterface *c, float *inputBuffers, float *outputBuffers, int n) {
  hv_assert(c != nullptr);
  return c->processInlineInterleaved(inputBuffers, outputBuffers, n);
}

HV_EXPORT void hv_delete(HeavyContextInterface *c) {
  delete c;
}

#ifdef __cplusplus
}
#endif
