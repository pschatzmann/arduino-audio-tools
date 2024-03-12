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

#ifndef _HEAVY_LIGHTPIPE_H_
#define _HEAVY_LIGHTPIPE_H_

#include "HvUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This pipe assumes that there is only one producer thread and one consumer
 * thread. This data structure does not support any other configuration.
 */
typedef struct HvLightPipe {
  char *buffer;
  char *writeHead;
  char *readHead;
  hv_uint32_t len;
  hv_uint32_t remainingBytes; // total bytes from write head to end
} HvLightPipe;

/**
 * Initialise the pipe with a given length, in bytes.
 * @return  Returns the size of the pipe in bytes.
 */
hv_uint32_t hLp_init(HvLightPipe *q, hv_uint32_t numBytes);

/**
 * Frees the internal buffer.
 * @param q  The light pipe.
 */
void hLp_free(HvLightPipe *q);

/**
 * Indicates if data is available for reading.
 * @param q  The light pipe.
 *
 * @return Returns the number of bytes available for reading. Zero if no bytes
 *         are available.
 */
hv_uint32_t hLp_hasData(HvLightPipe *q);

/**
 * Returns a pointer to a location in the pipe where numBytes can be written.
 *
 * @param numBytes  The number of bytes to be written.
 * @return  A pointer to a location where those bytes can be written. Returns
 *          NULL if no more space is available. Successive calls to this
 *          function may eventually return a valid pointer because the readhead
 *          has been advanced on another thread.
 */
char *hLp_getWriteBuffer(HvLightPipe *q, hv_uint32_t numBytes);

/**
 * Indicates to the pipe how many bytes have been written.
 *
 * @param numBytes  The number of bytes written. In general this should be the
 *                  same value as was passed to the preceeding call to
 *                  hLp_getWriteBuffer().
 */
void hLp_produce(HvLightPipe *q, hv_uint32_t numBytes);

/**
 * Returns the current read buffer, indicating the number of bytes available
 * for reading.
 * @param q  The light pipe.
 * @param numBytes  This value will be filled with the number of bytes available
 *                  for reading.
 *
 * @return  A pointer to the read buffer.
 */
char *hLp_getReadBuffer(HvLightPipe *q, hv_uint32_t *numBytes);

/**
 * Indicates that the next set of bytes have been read and are no longer needed.
 * @param q  The light pipe.
 */
void hLp_consume(HvLightPipe *q);

// resets the queue to it's initialised state
// This should be done when only one thread is accessing the pipe.
void hLp_reset(HvLightPipe *q);

#ifdef __cplusplus
}
#endif

#endif // _HEAVY_LIGHTPIPE_H_
