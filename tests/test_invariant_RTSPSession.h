#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Self-contained simulation of the vulnerable RTSP session handler pattern.
 * We model the buffer as a fixed-capacity vector-like structure and implement
 * a safe_rtsp_copy() that enforces the invariant:
 *   "Buffer reads never exceed the declared length"
 *
 * The invariant: any copy into mCurRequest must be bounded by the buffer's
 * allocated capacity. If aRequestSize > capacity, the operation must be
 * rejected (return error) rather than overflow the buffer.
 */

#define RTSP_BUFFER_CAPACITY 4096

typedef struct {
    uint8_t data[RTSP_BUFFER_CAPACITY];
    size_t  capacity;
    size_t  used;
} RTSPRequestBuffer;

/* Safe version of the copy — returns 0 on success, -1 if oversized */
static int safe_rtsp_copy(RTSPRequestBuffer *buf,
                          const uint8_t *aRequest,
                          size_t aRequestSize)
{
    if (buf == NULL || aRequest == NULL) {
        return -1;
    }
    /* Invariant enforcement: reject if size exceeds capacity */
    if (aRequestSize > buf->capacity) {
        return -1;  /* reject oversized input */
    }
    memcpy(buf->data, aRequest, aRequestSize);
    buf->used = aRequestSize;
    return 0;
}

/* Helper: build a payload of given length filled with a pattern byte */
static uint8_t *make_payload(size_t len, uint8_t fill)
{
    uint8_t *p = (uint8_t *)malloc(len);
    if (p) {
        memset(p, fill, len);
    }
    return p;
}

/* ------------------------------------------------------------------ */

START_TEST(test_rtsp_buffer_no_overread)
{
    /*
     * Invariant: Buffer reads never exceed the declared length.
     * For every input size, safe_rtsp_copy must either:
     *   (a) succeed and write exactly aRequestSize bytes (when size <= capacity), OR
     *   (b) reject the request (return -1) when size > capacity.
     * In no case may more than RTSP_BUFFER_CAPACITY bytes be written.
     */

    /* Sizes to test: normal, boundary, 2x, 10x, max size_t tricks */
    const size_t test_sizes[] = {
        0,
        1,
        64,
        512,
        RTSP_BUFFER_CAPACITY - 1,   /* one under capacity */
        RTSP_BUFFER_CAPACITY,        /* exactly capacity */
        RTSP_BUFFER_CAPACITY + 1,    /* one over */
        RTSP_BUFFER_CAPACITY * 2,    /* 2x oversized */
        RTSP_BUFFER_CAPACITY * 10,   /* 10x oversized */
        65536,
        131072,
        1048576,                     /* 1 MB */
    };
    int num_sizes = (int)(sizeof(test_sizes) / sizeof(test_sizes[0]));

    /* Adversarial string payloads (RTSP-like attack strings) */
    const char *string_payloads[] = {
        /* Normal RTSP request */
        "OPTIONS rtsp://example.com/stream RTSP/1.0\r\nCSeq: 1\r\n\r\n",
        /* Oversized method field */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
        /* Format string attempt */
        "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%n%n%n%n",
        /* Null bytes embedded */
        "OPTIONS\x00\x00\x00 rtsp://evil.com RTSP/1.0\r\n\r\n",
        /* Very long URI */
        "DESCRIBE rtsp://"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        ".com/stream RTSP/1.0\r\n\r\n",
        /* Content-Length header with huge value */
        "ANNOUNCE rtsp://x.com/y RTSP/1.0\r\nContent-Length: 9999999999\r\n\r\n",
        /* Heap spray pattern */
        "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90"
        "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90",
        /* Shell metacharacters */
        "OPTIONS `id` rtsp://$(whoami).evil.com/ RTSP/1.0\r\n\r\n",
        /* Unicode / UTF-8 overlong sequences */
        "\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf",
        /* CRLF injection */
        "OPTIONS rtsp://x.com/ RTSP/1.0\r\nX-Evil: foo\r\n\r\nGET / HTTP/1.1\r\n\r\n",
    };
    int num_string_payloads = (int)(sizeof(string_payloads) / sizeof(string_payloads[0]));

    RTSPRequestBuffer buf;
    buf.capacity = RTSP_BUFFER_CAPACITY;

    /* --- Test 1: size-based payloads --- */
    for (int i = 0; i < num_sizes; i++) {
        size_t sz = test_sizes[i];

        /* Allocate a payload of exactly sz bytes */
        uint8_t *payload = NULL;
        if (sz > 0) {
            payload = make_payload(sz, (uint8_t)(0x41 + (i % 26)));
            ck_assert_ptr_nonnull(payload);
        }

        memset(&buf.data, 0, RTSP_BUFFER_CAPACITY);
        buf.used = 0;

        int result = safe_rtsp_copy(&buf, payload ? payload : (const uint8_t *)"", sz);

        if (sz <= RTSP_BUFFER_CAPACITY) {
            /* Must succeed */
            ck_assert_int_eq(result, 0);
            /* used must equal sz */
            ck_assert_uint_eq(buf.used, sz);
            /* Verify data integrity for small sizes */
            if (sz > 0 && sz <= RTSP_BUFFER_CAPACITY) {
                ck_assert_int_eq(memcmp(buf.data, payload, sz), 0);
            }
        } else {
            /* Must be rejected — no overflow */
            ck_assert_int_eq(result, -1);
            /* used must remain 0 (no partial write) */
            ck_assert_uint_eq(buf.used, 0);
        }

        /* Invariant: used must never exceed capacity */
        ck_assert_uint_le(buf.used, buf.capacity);

        free(payload);
    }

    /* --- Test 2: string-based adversarial payloads --- */
    for (int i = 0; i < num_string_payloads; i++) {
        size_t sz = strlen(string_payloads[i]);

        memset(&buf.data, 0, RTSP_BUFFER_CAPACITY);
        buf.used = 0;

        int result = safe_rtsp_copy(&buf,
                                    (const uint8_t *)string_payloads[i],
                                    sz);

        if (sz <= RTSP_BUFFER_CAPACITY) {
            ck_assert_int_eq(result, 0);
            ck_assert_uint_eq(buf.used, sz);
        } else {
            ck_assert_int_eq(result, -1);
            ck_assert_uint_eq(buf.used, 0);
        }

        /* Core invariant: used never exceeds capacity */
        ck_assert_uint_le(buf.used, buf.capacity);
    }

    /* --- Test 3: boundary stress — sizes around capacity --- */
    for (size_t delta = 0; delta <= 16; delta++) {
        /* Below capacity */
        if (delta <= RTSP_BUFFER_CAPACITY) {
            size_t sz = RTSP_BUFFER_CAPACITY - delta;
            uint8_t *p = make_payload(sz, 0xBB);
            ck_assert_ptr_nonnull(p);

            memset(&buf.data, 0, RTSP_BUFFER_CAPACITY);
            buf.used = 0;

            int r = safe_rtsp_copy(&buf, p, sz);
            ck_assert_int_eq(r, 0);
            ck_assert_uint_eq(buf.used, sz);
            ck_assert_uint_le(buf.used, buf.capacity);
            free(p);
        }

        /* Above capacity */
        size_t sz2 = RTSP_BUFFER_CAPACITY + delta + 1;
        uint8_t *p2 = make_payload(sz2, 0xCC);
        ck_assert_ptr_nonnull(p2);

        memset(&buf.data, 0, RTSP_BUFFER_CAPACITY);
        buf.used = 0;

        int r2 = safe_rtsp_copy(&buf, p2, sz2);
        ck_assert_int_eq(r2, -1);
        ck_assert_uint_eq(buf.used, 0);
        ck_assert_uint_le(buf.used, buf.capacity);
        free(p2);
    }

    /* --- Test 4: NULL / degenerate inputs --- */
    {
        memset(&buf.data, 0, RTSP_BUFFER_CAPACITY);
        buf.used = 0;

        /* NULL request pointer */
        int r = safe_rtsp_copy(&buf, NULL, 100);
        ck_assert_int_eq(r, -1);
        ck_assert_uint_eq(buf.used, 0);

        /* NULL buffer */
        uint8_t dummy[8] = {0};
        r = safe_rtsp_copy(NULL, dummy, 8);
        ck_assert_int_eq(r, -1);

        /* Zero size */
        r = safe_rtsp_copy(&buf, dummy, 0);
        ck_assert_int_eq(r, 0);
        ck_assert_uint_eq(buf.used, 0);
        ck_assert_uint_le(buf.used, buf.capacity);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_set_timeout(tc_core, 60);
    tcase_add_test(tc_core, test_rtsp_buffer_no_overread);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}