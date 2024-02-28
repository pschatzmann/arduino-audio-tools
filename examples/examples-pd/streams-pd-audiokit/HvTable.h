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

#ifndef _HEAVY_TABLE_H_
#define _HEAVY_TABLE_H_

#include "HvHeavy.h"
#include "HvUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HvTable {
  float *buffer;
  // the number of values that the table is requested to have
  hv_uint32_t length;

  // the number of usable values that the table actually has
  // this is always an even multiple of HV_N_SIMD
  hv_uint32_t size;

  // Note that the true size of the table is (size + HV_N_SIMD),
  // with the trailing values used by the system, e.g. to create a circular
  // buffer
  hv_uint32_t allocated;

  hv_uint32_t head; // the most recently written point
} HvTable;

hv_size_t hTable_init(HvTable *o, int length);

hv_size_t hTable_initWithData(HvTable *o, int length, const float *data);

hv_size_t hTable_initWithFinalData(HvTable *o, int length, float *data);

void hTable_free(HvTable *o);

int hTable_resize(HvTable *o, hv_uint32_t newLength);

void hTable_onMessage(HeavyContextInterface *_c, HvTable *o, int letIn, const HvMessage *m,
    void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *));

static inline float *hTable_getBuffer(HvTable *o) {
  return o->buffer;
}

// the user-requested length of the table (number of floats)
static inline hv_uint32_t hTable_getLength(HvTable *o) {
  return o->length;
}

// the usable length of the table (an even multiple of HV_N_SIMD)
static inline hv_uint32_t hTable_getSize(HvTable *o) {
  return o->size;
}

// the number of floats allocated to this table (usually size + HV_N_SIMD)
static inline hv_uint32_t hTable_getAllocated(HvTable *o) {
  return o->allocated;
}

static inline hv_uint32_t hTable_getHead(HvTable *o) {
  return o->head;
}

static inline void hTable_setHead(HvTable *o, hv_uint32_t head) {
  o->head = head;
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _HEAVY_TABLE_H_
