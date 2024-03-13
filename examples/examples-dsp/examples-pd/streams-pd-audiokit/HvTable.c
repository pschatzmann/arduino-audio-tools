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

#include "HvTable.h"
#include "HvMessage.h"

hv_size_t hTable_init(HvTable *o, int length) {
  o->length = length;
  // true size of the table is always an integer multple of HV_N_SIMD
  o->size = (length + HV_N_SIMD_MASK) & ~HV_N_SIMD_MASK;
  // add an extra length for mirroring
  o->allocated = o->size + HV_N_SIMD;
  o->head = 0;
  hv_size_t numBytes = o->allocated * sizeof(float);
  o->buffer = (float *) hv_malloc(numBytes);
  hv_assert(o->buffer != NULL);
  hv_memclear(o->buffer, numBytes);
  return numBytes;
}

hv_size_t hTable_initWithData(HvTable *o, int length, const float *data) {
  o->length = length;
  o->size = (length + HV_N_SIMD_MASK) & ~HV_N_SIMD_MASK;
  o->allocated = o->size + HV_N_SIMD;
  o->head = 0;
  hv_size_t numBytes = o->size * sizeof(float);
  o->buffer = (float *) hv_malloc(numBytes);
  hv_assert(o->buffer != NULL);
  hv_memclear(o->buffer, numBytes);
  hv_memcpy(o->buffer, data, length*sizeof(float));
  return numBytes;
}

hv_size_t hTable_initWithFinalData(HvTable *o, int length, float *data) {
  o->length = length;
  o->size = length;
  o->allocated = length;
  o->buffer = data;
  o->head = 0;
  return 0;
}

void hTable_free(HvTable *o) {
  hv_free(o->buffer);
}

int hTable_resize(HvTable *o, hv_uint32_t newLength) {
  // TODO(mhroth): update context with memory allocated by table
  // NOTE(mhroth): mirrored bytes are not necessarily carried over
  const hv_uint32_t newSize = (newLength + HV_N_SIMD_MASK) & ~HV_N_SIMD_MASK;
  if (newSize == o->size) return 0; // early exit if no change in size
  const hv_uint32_t oldSizeBytes = (hv_uint32_t) (o->size * sizeof(float));
  const hv_uint32_t newAllocated = newSize + HV_N_SIMD;
  const hv_uint32_t newAllocatedBytes = (hv_uint32_t) (newAllocated * sizeof(float));

  float *b = (float *) hv_realloc(o->buffer, newAllocatedBytes);
  hv_assert(b != NULL); // error while reallocing!
  // ensure that hv_realloc has given us a correctly aligned buffer
  if ((((hv_uintptr_t) (const void *) b) & ((0x1<<HV_N_SIMD)-1)) == 0) {
    if (newSize > o->size) {
      hv_memclear(b + o->size, (newAllocated - o->size) * sizeof(float)); // clear new parts of the buffer
    }
    o->buffer = b;
  } else {
    // if not, we have to re-malloc ourselves
    char *c = (char *) hv_malloc(newAllocatedBytes);
    hv_assert(c != NULL); // error while allocating new buffer!
    if (newAllocatedBytes > oldSizeBytes) {
      hv_memcpy(c, b, oldSizeBytes);
      hv_memclear(c + oldSizeBytes, newAllocatedBytes - oldSizeBytes);
    } else {
      hv_memcpy(c, b, newAllocatedBytes);
    }
    hv_free(b);
    o->buffer = (float *) c;
  }
  o->length = newLength;
  o->size = newSize;
  o->allocated = newAllocated;
  return (int) (newAllocated - oldSizeBytes - (HV_N_SIMD*sizeof(float)));
}

void hTable_onMessage(HeavyContextInterface *_c, HvTable *o, int letIn, const HvMessage *m,
    void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *)) {
  if (msg_compareSymbol(m,0,"resize") && msg_isFloat(m,1) && msg_getFloat(m,1) >= 0.0f) {
    hTable_resize(o, (int) hv_ceil_f(msg_getFloat(m,1))); // apply ceil to ensure that tables always have enough space

    // send out the new size of the table
    HvMessage *n = HV_MESSAGE_ON_STACK(1);
    msg_initWithFloat(n, msg_getTimestamp(m), (float) hTable_getSize(o));
    sendMessage(_c, 0, n);
  }

  else if (msg_compareSymbol(m,0,"mirror")) {
    hv_memcpy(o->buffer+o->size, o->buffer, HV_N_SIMD*sizeof(float));
  }
}
