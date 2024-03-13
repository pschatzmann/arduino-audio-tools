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

#ifndef _HEAVY_H_
#define _HEAVY_H_

#include "HvUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _HEAVY_DECLARATIONS_
#define _HEAVY_DECLARATIONS_

#ifdef __cplusplus
class HeavyContextInterface;
#else
typedef struct HeavyContextInterface HeavyContextInterface;
#endif

typedef struct HvMessage HvMessage;

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



#if HV_APPLE
#pragma mark - Heavy Context
#endif

/** Deletes a patch instance. */
void hv_delete(HeavyContextInterface *c);



#if HV_APPLE
#pragma mark - Heavy Process
#endif

/**
 * Processes one block of samples for a patch instance. The buffer format is an array of float channel arrays.
 * If the context has not input or output channels, the respective argument may be NULL.
 * The number of samples to to tbe processed should be a multiple of 1, 4, or 8, depending on if
 * no, SSE or NEON, or AVX optimisation is being used, respectively.
 * e.g. [[LLLL][RRRR]]
 * This function support in-place processing.
 *
 * @return  The number of samples processed.
 *
 * This function is NOT thread-safe. It is assumed that only the audio thread will execute this function.
 */
int hv_process(HeavyContextInterface *c, float **inputBuffers, float **outputBuffers, int n);

/**
 * Processes one block of samples for a patch instance. The buffer format is an uninterleaved float array of channels.
 * If the context has not input or output channels, the respective argument may be NULL.
 * The number of samples to to tbe processed should be a multiple of 1, 4, or 8, depending on if
 * no, SSE or NEON, or AVX optimisation is being used, respectively.
 * e.g. [LLLLRRRR]
 * This function support in-place processing.
 *
 * @return  The number of samples processed.
 *
 * This function is NOT thread-safe. It is assumed that only the audio thread will execute this function.
 */
int hv_processInline(HeavyContextInterface *c, float *inputBuffers, float *outputBuffers, int n);

/**
 * Processes one block of samples for a patch instance. The buffer format is an interleaved float array of channels.
 * If the context has not input or output channels, the respective argument may be NULL.
 * The number of samples to to tbe processed should be a multiple of 1, 4, or 8, depending on if
 * no, SSE or NEON, or AVX optimisation is being used, respectively.
 * e.g. [LRLRLRLR]
 * This function support in-place processing.
 *
 * @return  The number of samples processed.
 *
 * This function is NOT thread-safe. It is assumed that only the audio thread will execute this function.
 */
int hv_processInlineInterleaved(HeavyContextInterface *c, float *inputBuffers, float *outputBuffers, int n);



#if HV_APPLE
#pragma mark - Heavy Common
#endif

/**
 * Returns the total size in bytes of the context.
 * This value may change if tables are resized.
 */
int hv_getSize(HeavyContextInterface *c);

/** Returns the sample rate with which this context has been configured. */
double hv_getSampleRate(HeavyContextInterface *c);

/** Returns the number of input channels with which this context has been configured. */
int hv_getNumInputChannels(HeavyContextInterface *c);

/** Returns the number of output channels with which this context has been configured. */
int hv_getNumOutputChannels(HeavyContextInterface *c);

/** Set the print hook. The function is called whenever a message is sent to a print object. */
void hv_setPrintHook(HeavyContextInterface *c, HvPrintHook_t *f);

/** Returns the print hook, or NULL. */
HvPrintHook_t *hv_getPrintHook(HeavyContextInterface *c);

/**
 * Set the send hook. The function is called whenever a message is sent to any send object.
 * Messages returned by this function should NEVER be freed. If the message must persist, call
 * hv_msg_copy() first.
 */
void hv_setSendHook(HeavyContextInterface *c, HvSendHook_t *f);

/** Returns a 32-bit hash of any string. Returns 0 if string is NULL. */
hv_uint32_t hv_stringToHash(const char *s);

/**
 * A convenience function to send a bang to a receiver to be processed immediately.
 * The receiver is addressed with its hash, which can also be determined using hv_stringToHash().
 * This function is thread-safe.
 *
 * @return  True if the message was accepted. False if the message could not fit onto
 *          the message queue to be processed this block.
 */
bool hv_sendBangToReceiver(HeavyContextInterface *c, hv_uint32_t receiverHash);

/**
 * A convenience function to send a float to a receiver to be processed immediately.
 * The receiver is addressed with its hash, which can also be determined using hv_stringToHash().
 * This function is thread-safe.
 *
 * @return  True if the message was accepted. False if the message could not fit onto
 *          the message queue to be processed this block.
 */
bool hv_sendFloatToReceiver(HeavyContextInterface *c, hv_uint32_t receiverHash, const float x);

/**
 * A convenience function to send a symbol to a receiver to be processed immediately.
 * The receiver is addressed with its hash, which can also be determined using hv_stringToHash().
 * This function is thread-safe.
 *
 * @return  True if the message was accepted. False if the message could not fit onto
 *          the message queue to be processed this block.
 */
bool hv_sendSymbolToReceiver(HeavyContextInterface *c, hv_uint32_t receiverHash, char *s);

/**
 * Sends a formatted message to a receiver that can be scheduled for the future.
 * The receiver is addressed with its hash, which can also be determined using hv_stringToHash().
 * This function is thread-safe.
 *
 * @return  True if the message was accepted. False if the message could not fit onto
 *          the message queue to be processed this block.
 */
bool hv_sendMessageToReceiverV(HeavyContextInterface *c, hv_uint32_t receiverHash, double delayMs, const char *format, ...);

/**
 * Sends a message to a receiver that can be scheduled for the future.
 * The receiver is addressed with its hash, which can also be determined using hv_stringToHash().
 * This function is thread-safe.
 *
 * @return  True if the message was accepted. False if the message could not fit onto
 *          the message queue to be processed this block.
 */
bool hv_sendMessageToReceiver(HeavyContextInterface *c, hv_uint32_t receiverHash, double delayMs, HvMessage *m);

/**
 * Cancels a previously scheduled message.
 *
 * @param sendMessage  May be NULL.
 */
void hv_cancelMessage(HeavyContextInterface *c, HvMessage *m, void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *));

/** Returns the read-only user-assigned name of this patch. */
const char *hv_getName(HeavyContextInterface *c);

/** Sets a user-definable value. This value is never manipulated by Heavy. */
void hv_setUserData(HeavyContextInterface *c, void *userData);

/** Returns the user-defined data. */
void *hv_getUserData(HeavyContextInterface *c);

/** Returns the current patch time in milliseconds. This value may have rounding errors. */
double hv_getCurrentTime(HeavyContextInterface *c);

/** Returns the current patch time in samples. This value is always exact. */
hv_uint32_t hv_getCurrentSample(HeavyContextInterface *c);

/**
 * Returns information about each parameter such as name, hash, and range.
 * The total number of parameters is always returned.
 *
 * @param index  The parameter index.
 * @param info  A pointer to a HvParameterInfo struct. May be null.
 *
 * @return  The total number of parameters.
 */
int hv_getParameterInfo(HeavyContextInterface *c, int index, HvParameterInfo *info);

/** */
float hv_samplesToMilliseconds(HeavyContextInterface *c, hv_uint32_t numSamples);

/** Converts milliseconds to samples. Input is limited to non-negative range. */
hv_uint32_t hv_millisecondsToSamples(HeavyContextInterface *c, float ms);

/**
 * Acquire the input message queue lock.
 *
 * This function will block until the message lock as been acquired.
 * Typical applications will not require the use of this function.
 *
 * @param c  A Heavy context.
 */
void hv_lock_acquire(HeavyContextInterface *c);

/**
 * Try to acquire the input message queue lock.
 *
 * If the lock has been acquired, hv_lock_release() must be called to release it.
 * Typical applications will not require the use of this function.
 *
 * @param c  A Heavy context.
 *
 * @return Returns true if the lock has been acquired, false otherwise.
 */
bool hv_lock_try(HeavyContextInterface *c);

/**
 * Release the input message queue lock.
 *
 * Typical applications will not require the use of this function.
 *
 * @param c  A Heavy context.
 */
void hv_lock_release(HeavyContextInterface *c);

/**
 * Set the size of the input message queue in kilobytes.
 *
 * The buffer is reset and all existing contents are lost on resize.
 *
 * @param c  A Heavy context.
 * @param inQueueKb  Must be positive i.e. at least one.
 */
void hv_setInputMessageQueueSize(HeavyContextInterface *c, hv_uint32_t inQueueKb);

/**
 * Set the size of the output message queue in kilobytes.
 *
 * The buffer is reset and all existing contents are lost on resize.
 * Only the default sendhook uses the outgoing message queue. If the default
 * sendhook is not being used, then this function is not useful.
 *
 * @param c  A Heavy context.
 * @param outQueueKb  Must be postive i.e. at least one.
 */
void hv_setOutputMessageQueueSize(HeavyContextInterface *c, hv_uint32_t outQueueKb);

/**
 * Get the next message in the outgoing queue, will also consume the message.
 * Returns false if there are no messages.
 *
 * @param c  A Heavy context.
 * @param destinationHash  a hash of the name of the receiver the message was sent to.
 * @param outMsg  message pointer that is filled by the next message contents.
 * @param msgLength  length of outMsg in bytes.
 *
 * @return  True if there is a message in the outgoing queue.
*/
bool hv_getNextSentMessage(HeavyContextInterface *c, hv_uint32_t *destinationHash, HvMessage *outMsg, hv_uint32_t msgLength);



#if HV_APPLE
#pragma mark - Heavy Message
#endif

typedef struct HvMessage HvMessage;

/** Returns the total size in bytes of a HvMessage with a number of elements on the heap. */
unsigned long hv_msg_getByteSize(hv_uint32_t numElements);

/** Initialise a HvMessage structure with the number of elements and a timestamp (in samples). */
void hv_msg_init(HvMessage *m, int numElements, hv_uint32_t timestamp);

/** Returns the number of elements in this message. */
unsigned long hv_msg_getNumElements(const HvMessage *m);

/** Returns the time at which this message exists (in samples). */
hv_uint32_t hv_msg_getTimestamp(const HvMessage *m);

/** Set the time at which this message should be executed (in samples). */
void hv_msg_setTimestamp(HvMessage *m, hv_uint32_t timestamp);

/** Returns true of the indexed element is a bang. False otherwise. Index is not bounds checked. */
bool hv_msg_isBang(const HvMessage *const m, int i);

/** Sets the indexed element to a bang. Index is not bounds checked. */
void hv_msg_setBang(HvMessage *m, int i);

/** Returns true of the indexed element is a float. False otherwise. Index is not bounds checked. */
bool hv_msg_isFloat(const HvMessage *const m, int i);

/** Returns the indexed element as a float value. Index is not bounds checked. */
float hv_msg_getFloat(const HvMessage *const m, int i);

/** Sets the indexed element to float value. Index is not bounds checked. */
void hv_msg_setFloat(HvMessage *m, int i, float f);

/** Returns true of the indexed element is a symbol. False otherwise. Index is not bounds checked. */
bool hv_msg_isSymbol(const HvMessage *const m, int i);

/** Returns the indexed element as a symbol value. Index is not bounds checked. */
const char *hv_msg_getSymbol(const HvMessage *const m, int i);

/** Returns true of the indexed element is a hash. False otherwise. Index is not bounds checked. */
bool hv_msg_isHash(const HvMessage *const m, int i);

/** Returns the indexed element as a hash value. Index is not bounds checked. */
hv_uint32_t hv_msg_getHash(const HvMessage *const m, int i);

/** Sets the indexed element to symbol value. Index is not bounds checked. */
void hv_msg_setSymbol(HvMessage *m, int i, const char *s);

/**
 * Returns true if the message has the given format, in number of elements and type. False otherwise.
 * Valid element types are:
 * 'b': bang
 * 'f': float
 * 's': symbol
 *
 * For example, a message with three floats would have a format of "fff". A single bang is "b".
 * A message with two symbols is "ss". These types can be mixed and matched in any way.
 */
bool hv_msg_hasFormat(const HvMessage *const m, const char *fmt);

/**
 * Returns a basic string representation of the message.
 * The character array MUST be deallocated by the caller.
 */
char *hv_msg_toString(const HvMessage *const m);

/** Copy a message onto the stack. The message persists. */
HvMessage *hv_msg_copy(const HvMessage *const m);

/** Free a copied message. */
void hv_msg_free(HvMessage *m);



#if HV_APPLE
#pragma mark - Heavy Table
#endif

/**
 * Resizes the table to the given length.
 *
 * Existing contents are copied to the new table. Remaining space is cleared
 * if the table is longer than the original, truncated otherwise.
 *
 * @param tableHash  The table identifier.
 * @param newSampleLength  The new length of the table, in samples. Must be positive.
 *
 * @return  False if the table could not be found. True otherwise.
 */
bool hv_table_setLength(HeavyContextInterface *c, hv_uint32_t tableHash, hv_uint32_t newSampleLength);

/** Returns a pointer to the raw buffer backing this table. DO NOT free it. */
float *hv_table_getBuffer(HeavyContextInterface *c, hv_uint32_t tableHash);

/** Returns the length of this table in samples. */
hv_uint32_t hv_table_getLength(HeavyContextInterface *c, hv_uint32_t tableHash);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _HEAVY_H_
