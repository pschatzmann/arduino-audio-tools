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

#include "HvLightPipe.h"

#if __SSE__ || HV_SIMD_SSE
#include <xmmintrin.h>
#define hv_sfence() _mm_sfence()
#elif __arm__ || HV_SIMD_NEON
  #if __ARM_ACLE
    #include <arm_acle.h>
    // https://msdn.microsoft.com/en-us/library/hh875058.aspx#BarrierRestrictions
    // http://doxygen.reactos.org/d8/d47/armintr_8h_a02be7ec76ca51842bc90d9b466b54752.html
    #define hv_sfence() __dmb(0xE) /* _ARM_BARRIER_ST */
  #elif defined(__GNUC__)
    #define hv_sfence() __asm__ volatile ("dmb 0xE":::"memory")
  #else
    // http://stackoverflow.com/questions/19965076/gcc-memory-barrier-sync-synchronize-vs-asm-volatile-memory
    #define hv_sfence() __sync_synchronize()
  #endif
#elif HV_WIN
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms684208(v=vs.85).aspx
#define hv_sfence() _WriteBarrier()
#else
#define hv_sfence() __asm__ volatile("" : : : "memory")
#endif

#define HLP_STOP 0
#define HLP_LOOP 0xFFFFFFFF
#define HLP_SET_UINT32_AT_BUFFER(a, b) (*((hv_uint32_t *) (a)) = (b))
#define HLP_GET_UINT32_AT_BUFFER(a) (*((hv_uint32_t *) (a)))

hv_uint32_t hLp_init(HvLightPipe *q, hv_uint32_t numBytes) {
  if (numBytes > 0) {
    q->buffer = (char *) hv_malloc(numBytes);
    hv_assert(q->buffer != NULL);
    HLP_SET_UINT32_AT_BUFFER(q->buffer, HLP_STOP);
  } else {
    q->buffer = NULL;
  }
  q->writeHead = q->buffer;
  q->readHead = q->buffer;
  q->len = numBytes;
  q->remainingBytes = numBytes;
  return numBytes;
}

void hLp_free(HvLightPipe *q) {
  hv_free(q->buffer);
}

hv_uint32_t hLp_hasData(HvLightPipe *q) {
  hv_uint32_t x = HLP_GET_UINT32_AT_BUFFER(q->readHead);
  if (x == HLP_LOOP) {
    q->readHead = q->buffer;
    x = HLP_GET_UINT32_AT_BUFFER(q->readHead);
  }
  return x;
}

char *hLp_getWriteBuffer(HvLightPipe *q, hv_uint32_t bytesToWrite) {
  char *const readHead = q->readHead;
  char *const oldWriteHead = q->writeHead;
  const hv_uint32_t totalByteRequirement = bytesToWrite + 2*sizeof(hv_uint32_t);

  // check if there is enough space to write the data in the remaining
  // length of the buffer
  if (totalByteRequirement <= q->remainingBytes) {
    char *const newWriteHead = oldWriteHead + sizeof(hv_uint32_t) + bytesToWrite;

    // check if writing would overwrite existing data in the pipe (return NULL if so)
    if ((oldWriteHead < readHead) && (newWriteHead >= readHead)) return NULL;
    else return (oldWriteHead + sizeof(hv_uint32_t));
  } else {
    // there isn't enough space, try looping around to the start
    if (totalByteRequirement <= q->len) {
      if ((oldWriteHead < readHead) || ((q->buffer + totalByteRequirement) > readHead)) {
        return NULL; // overwrite condition
      } else {
        q->writeHead = q->buffer;
        q->remainingBytes = q->len;
        HLP_SET_UINT32_AT_BUFFER(q->buffer, HLP_STOP);
        hv_sfence();
        HLP_SET_UINT32_AT_BUFFER(oldWriteHead, HLP_LOOP);
        return q->buffer + sizeof(hv_uint32_t);
      }
    } else {
      return NULL; // there isn't enough space to write the data
    }
  }
}

void hLp_produce(HvLightPipe *q, hv_uint32_t numBytes) {
  hv_assert(q->remainingBytes >= (numBytes + 2*sizeof(hv_uint32_t)));
  q->remainingBytes -= (sizeof(hv_uint32_t) + numBytes);
  char *const oldWriteHead = q->writeHead;
  q->writeHead += (sizeof(hv_uint32_t) + numBytes);
  HLP_SET_UINT32_AT_BUFFER(q->writeHead, HLP_STOP);

  // save everything before this point to memory
  hv_sfence();

  // then save this
  HLP_SET_UINT32_AT_BUFFER(oldWriteHead, numBytes);
}

char *hLp_getReadBuffer(HvLightPipe *q, hv_uint32_t *numBytes) {
  *numBytes = HLP_GET_UINT32_AT_BUFFER(q->readHead);
  char *const readBuffer = q->readHead + sizeof(hv_uint32_t);
  return readBuffer;
}

void hLp_consume(HvLightPipe *q) {
  hv_assert(HLP_GET_UINT32_AT_BUFFER(q->readHead) != HLP_STOP);
  q->readHead += sizeof(hv_uint32_t) + HLP_GET_UINT32_AT_BUFFER(q->readHead);
}

void hLp_reset(HvLightPipe *q) {
  q->writeHead = q->buffer;
  q->readHead = q->buffer;
  q->remainingBytes = q->len;
  memset(q->buffer, 0, q->len);
}
