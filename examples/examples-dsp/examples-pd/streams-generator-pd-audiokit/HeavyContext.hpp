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

#ifndef _HEAVY_CONTEXT_H_
#define _HEAVY_CONTEXT_H_

#include "HeavyContextInterface.hpp"
#include "HvLightPipe.h"
#include "HvMessageQueue.h"
#include "HvMath.h"

struct HvTable;

class HeavyContext : public HeavyContextInterface {

 public:
  HeavyContext(double sampleRate, int poolKb=10, int inQueueKb=2, int outQueueKb=0);
  virtual ~HeavyContext();

  int getSize() override { return (int) numBytes; }

  double getSampleRate() override { return sampleRate; }

  hv_uint32_t getCurrentSample() override { return blockStartTimestamp; }
  float samplesToMilliseconds(hv_uint32_t numSamples) override { return (float) (1000.0*numSamples/sampleRate); }
  hv_uint32_t millisecondsToSamples(float ms) override { return (hv_uint32_t) (hv_max_f(0.0f,ms)*sampleRate/1000.0); }

  void setUserData(void *x) override { userData = x; }
  void *getUserData() override { return userData; }

  // hook management
  void setSendHook(HvSendHook_t *f) override { sendHook = f; }
  HvSendHook_t *getSendHook() override { return sendHook; }

  void setPrintHook(HvPrintHook_t *f) override { printHook = f; }
  HvPrintHook_t *getPrintHook() override { return printHook; }

  // message scheduling
  bool sendMessageToReceiver(hv_uint32_t receiverHash, double delayMs, HvMessage *m) override;
  bool sendMessageToReceiverV(hv_uint32_t receiverHash, double delayMs, const char *fmt, ...) override;
  bool sendFloatToReceiver(hv_uint32_t receiverHash, float f) override;
  bool sendBangToReceiver(hv_uint32_t receiverHash) override;
  bool sendSymbolToReceiver(hv_uint32_t receiverHash, const char *symbol) override;
  bool cancelMessage(HvMessage *m, void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *)) override;

  // table manipulation
  float *getBufferForTable(hv_uint32_t tableHash) override;
  int getLengthForTable(hv_uint32_t tableHash) override;
  bool setLengthForTable(hv_uint32_t tableHash, hv_uint32_t newSampleLength) override;

  // lock control
  void lockAcquire() override;
  bool lockTry() override;
  void lockRelease() override;

  // message queue management
  void setInputMessageQueueSize(int inQueueKb) override;
  void setOutputMessageQueueSize(int outQueueKb) override;
  bool getNextSentMessage(hv_uint32_t *destinationHash, HvMessage *outMsg, hv_size_t msgLength) override;

  // utility functions
  static hv_uint32_t getHashForString(const char *str);

 protected:
  virtual HvTable *getTableForHash(hv_uint32_t tableHash) = 0;
  friend HvTable *_hv_table_get(HeavyContextInterface *, hv_uint32_t);

  virtual void scheduleMessageForReceiver(hv_uint32_t receiverHash, HvMessage *m) = 0;
  friend void _hv_scheduleMessageForReceiver(HeavyContextInterface *, hv_uint32_t, HvMessage *);

  HvMessage *scheduleMessageForObject(const HvMessage *,
      void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *),
      int);
  friend HvMessage *_hv_scheduleMessageForObject(HeavyContextInterface *, const HvMessage *,
      void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *),
      int);

  friend void defaultSendHook(HeavyContextInterface *, const char *, hv_uint32_t, const HvMessage *);

  // object state
  double sampleRate;
  hv_uint32_t blockStartTimestamp;
  hv_size_t numBytes;
  HvMessageQueue mq;
  HvSendHook_t *sendHook;
  HvPrintHook_t *printHook;
  void *userData;
  HvLightPipe inQueue;
  HvLightPipe outQueue;
  hv_atomic_bool inQueueLock;
  hv_atomic_bool outQueueLock;
};

#endif // _HEAVY_CONTEXT_H_
