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

#ifndef _HEAVY_INTERNAL_H_
#define _HEAVY_INTERNAL_H_

#include "HvHeavy.h"
#include "HvUtils.h"
#include "HvTable.h"
#include "HvMessage.h"
#include "HvMath.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
HvTable *hv_table_get(HeavyContextInterface *c, hv_uint32_t tableHash);

/**
 *
 */
void hv_scheduleMessageForReceiver(HeavyContextInterface *c, hv_uint32_t receiverHash, HvMessage *m);

/**
 *
 */
HvMessage *hv_scheduleMessageForObject(HeavyContextInterface *c, const HvMessage *m,
    void (*sendMessage)(HeavyContextInterface *, int, const HvMessage *),
    int letIndex);

#ifdef __cplusplus
}
#endif

#endif
