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

#ifndef _HEAVY_CONTEXT_INTERFACE_H_
#define _HEAVY_CONTEXT_INTERFACE_H_

#include "HvUtils.h"

#ifndef _HEAVY_DECLARATIONS_
#define _HEAVY_DECLARATIONS_

class HeavyContextInterface;
struct HvMessage;

typedef enum {
  HV_PARAM_TYPE_PARAMETER_IN,
  HV_PARAM_TYPE_PARAMETER_OUT,
  HV_PARAM_TYPE_EVENT_IN,
  HV_PARAM_TYPE_EVENT_OUT
} HvParameterType;

typedef struct HvParameterInfo {
  const char *name;     // the human readable parameter name
  hv_uint32_t hash;     // an integer identified used by heavy for this parameter
  HvParameterType type; // type of this parameter
  float minVal;         // the minimum value of this parameter
  float maxVal;         // the maximum value of this parameter
  float defaultVal;     // the default value of this parameter
} HvParameterInfo;

typedef void (HvSendHook_t) (HeavyContextInterface *context, const char *sendName, hv_uint32_t sendHash, const HvMessage *msg);
typedef void (HvPrintHook_t) (HeavyContextInterface *context, const char *printName, const char *str, const HvMessage *msg);

#endif // _HEAVY_DECLARATIONS_



class HeavyContextInterface {

 public:
  HeavyContextInterface() {}
  virtual ~HeavyContextInterface() {};

  /** Returns the read-only user-assigned name of this patch. */
  virtual const char *getName() = 0;

  /** Returns the number of input channels with which this context has been configured. */
  virtual int getNumInputChannels() = 0;

  /** Returns the number of output channels with which this context has been configured. */
  virtual int getNumOutputChannels() = 0;

  /**
   * Returns the total size in bytes of the context.
   * This value may change if tables are resized.
   */
  virtual int getSize() = 0;

  /** Returns the sample rate with which this context has been configured. */
  virtual double getSampleRate() = 0;

  /** Returns the current patch time in samples. This value is always exact. */
  virtual hv_uint32_t getCurrentSample() = 0;
  virtual float samplesToMilliseconds(hv_uint32_t numSamples) = 0;

  /** Converts milliseconds to samples. Input is limited to non-negative range. */
  virtual hv_uint32_t millisecondsToSamples(float ms) = 0;

  /** Sets a user-definable value. This value is never manipulated by Heavy. */
  virtual void setUserData(void *x) = 0;

  /** Returns the user-defined data. */
  virtual void *getUserData() = 0;

  /**
   * Set the send hook. The function is called whenever a message is sent to any send object.
   * Messages returned by this function should NEVER be freed. If the message must persist, call
   * hv_msg_copy() first.
   */
  virtual void setSendHook(HvSendHook_t *f) = 0;

  /** Returns the send hook, or NULL if unset. */
  virtual HvSendHook_t *getSendHook() = 0;

  /** Set the print hook. The function is called whenever a message is sent to a print object. */
  virtual void setPrintHook(HvPrintHook_t *f) = 0;

  /** Returns the print hook, or NULL if unset. */
  virtual HvPrintHook_t *getPrintHook() = 0;

  /**
   * Processes one block of samples for a patch instance. The buffer format is an array of float channel arrays.
   * If the context has not input or output channels, the respective argument may be NULL.
   * The number of samples to to tbe processed should be a multiple of 1, 4, or 8, depending on if
   * no, SSE or NEON, or AVX optimisation is being used, respectively.
   * e.g. [[LLLL][RRRR]]
   *
   * @return  The number of samples processed.
   *
   * This function is NOT thread-safe. It is assumed that only the audio thread will execute this function.
   */
  virtual int process(float **inputBuffers, float **outputBuffer, int n) = 0;

  /**
   * Processes one block of samples for a patch instance. The buffer format is an uninterleaved float array of channels.
   * If the context has not input or output channels, the respective argument may be NULL.
   * The number of samples to to tbe processed should be a multiple of 1, 4, or 8, depending on if
   * no, SSE or NEON, or AVX optimisation is being used, respectively.
   * e.g. [LLLLRRRR]
   *
   * @return  The number of samples processed.
   *
   * This function is NOT thread-safe. It is assumed that only the audio thread will execute this function.
   */
  virtual int processInline(float *inputBuffers, float *outputBuffer, int n) = 0;

  /**
   * Processes one block of samples for a patch instance. The buffer format is an interleaved float array of channels.
   * If the context has not input or output channels, the respective argument may be NULL.
   * The number of samples to to tbe processed should be a multiple of 1, 4, or 8, depending on if
   * no, SSE or NEON, or AVX optimisation is being used, respectively.
   * e.g. [LRLRLRLR]
   *
   * @return  The number of samples processed.
   *
   * This function is NOT thread-safe. It is assumed that only the audio thread will execute this function.
   */
  virtual int processInlineInterleaved(float *inputBuffers, float *outputBuffer, int n) = 0;

  /**
   * Sends a formatted message to a receiver that can be scheduled for the future.
   * The receiver is addressed with its hash, which can also be determined using hv_stringToHash().
   * This function is thread-safe.
   *
   * @return  True if the message was accepted. False if the message could not fit onto
   *          the message queue to be processed this block.
   */
  virtual bool sendMessageToReceiver(hv_uint32_t receiverHash, double delayMs, HvMessage *m) = 0;

  /**
   * Sends a formatted message to a receiver that can be scheduled for the future.
   * The receiver is addressed with its hash, which can also be determined using hv_stringToHash().
   * This function is thread-safe.
   *
   * @return  True if the message was accepted. False if the message could not fit onto
   *          the message queue to be processed this block.
   */
  virtual bool sendMessageToReceiverV(hv_uint32_t receiverHash, double delayMs, const char *fmt, ...) = 0;

  /**
   * A convenience function to send a float to a receiver to be processed immediately.
   * The receiver is addressed with its hash, which can also be determined using hv_stringToHash().
   * This function is thread-safe.
   *
   * @return  True if the message was accepted. False if the message could not fit onto
   *          the message queue to be processed this block.
   */
  virtual bool sendFloatToReceiver(hv_uint32_t receiverHash, float f) = 0;

  /**
   * A convenience function to send a bang to a receiver to be processed immediately.
   * The receiver is addressed with its hash, which can also be determined using hv_stringToHash().
   * This function is thread-safe.
   *
   * @return  True if the message was accepted. False if the message could not fit onto
   *          the message queue to be processed this block.
   */
  virtual bool sendBangToReceiver(hv_uint32_t receiverHash) = 0;

  /**
   * A convenience function to send a symbol to a receiver to be processed immediately.
   * The receiver is addressed with its hash, which can also be determined using hv_stringToHash().
   * This function is thread-safe.
   *
   * @return  True if the message was accepted. False if the message could not fit onto
   *          the message queue to be processed this block.
   */
  virtual bool sendSymbolToReceiver(hv_uint32_t receiverHash, const char *symbol)  = 0;

  /**
   * Cancels a previously scheduled message.
   *
   * @param sendMessage  May be NULL.
   */
  virtual bool cancelMessage(HvMessage *m, void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *)=nullptr) = 0;

  /**
   * Returns information about each parameter such as name, hash, and range.
   * The total number of parameters is always returned.
   *
   * @param index  The parameter index.
   * @param info  A pointer to a HvParameterInfo struct. May be null.
   *
   * @return  The total number of parameters.
   */
  virtual int getParameterInfo(int index, HvParameterInfo *info) = 0;

  /** Returns a pointer to the raw buffer backing this table. DO NOT free it. */
  virtual float *getBufferForTable(hv_uint32_t tableHash) = 0;

  /** Returns the length of this table in samples. */
  virtual int getLengthForTable(hv_uint32_t tableHash) = 0;

  /**
   * Resizes the table to the given length.
   *
   * Existing contents are copied to the new table. Remaining space is cleared
   * if the table is longer than the original, truncated otherwise.
   *
   * @param tableHash  The table identifier.
   * @param newSampleLength  The new length of the table, in samples.
   *
   * @return  False if the table could not be found. True otherwise.
   */
  virtual bool setLengthForTable(hv_uint32_t tableHash, hv_uint32_t newSampleLength) = 0;

  /**
   * Acquire the input message queue lock.
   *
   * This function will block until the message lock as been acquired.
   * Typical applications will not require the use of this function.
   */
  virtual void lockAcquire() = 0;

  /**
   * Try to acquire the input message queue lock.
   *
   * If the lock has been acquired, hv_lock_release() must be called to release it.
   * Typical applications will not require the use of this function.
   *
   * @return Returns true if the lock has been acquired, false otherwise.
   */
  virtual bool lockTry() = 0;

  /**
   * Release the input message queue lock.
   *
   * Typical applications will not require the use of this function.
   */
  virtual void lockRelease() = 0;

  /**
   * Set the size of the input message queue in kilobytes.
   *
   * The buffer is reset and all existing contents are lost on resize.
   *
   * @param inQueueKb  Must be positive i.e. at least one.
   */
  virtual void setInputMessageQueueSize(int inQueueKb) = 0;

  /**
   * Set the size of the output message queue in kilobytes.
   *
   * The buffer is reset and all existing contents are lost on resize.
   * Only the default sendhook uses the outgoing message queue. If the default
   * sendhook is not being used, then this function is not useful.
   *
   * @param outQueueKb  Must be postive i.e. at least one.
   */
  virtual void setOutputMessageQueueSize(int outQueueKb) = 0;

  /**
   * Get the next message in the outgoing queue, will also consume the message.
   * Returns false if there are no messages.
   *
   * @param destinationHash  a hash of the name of the receiver the message was sent to.
   * @param outMsg  message pointer that is filled by the next message contents.
   * @param msgLengthBytes  max length of outMsg in bytes.
   *
   * @return  True if there is a message in the outgoing queue.
  */
  virtual bool getNextSentMessage(hv_uint32_t *destinationHash, HvMessage *outMsg, hv_size_t msgLengthBytes) = 0;

  /** Returns a 32-bit hash of any string. Returns 0 if string is NULL. */
  static hv_uint32_t getHashForString(const char *str);
};

#endif // _HEAVY_CONTEXT_INTERFACE_H_
