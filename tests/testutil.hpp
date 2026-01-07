/* ServerLink Test Utilities */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SERVERLINK_TESTUTIL_HPP
#define SERVERLINK_TESTUTIL_HPP

#include <serverlink/serverlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>  /* For _getpid */
#define getpid _getpid
#else
#include <unistd.h>
#endif

/* Forward declaration for test_endpoint_tcp (defined later in file) */
static inline const char* test_endpoint_tcp();

/* TCP settle time in milliseconds - time to wait for TCP connections to establish
 * and messages to propagate through the network. libzmq uses 300ms. */
#ifndef SETTLE_TIME
#define SETTLE_TIME 300
#endif

/* Test framework macros */
#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "ASSERTION FAILED: %s\n  at %s:%d\n", #cond, __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)

#define TEST_ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            fprintf(stderr, "ASSERTION FAILED: %s == %s (%d != %d)\n  at %s:%d\n", \
                    #a, #b, (int)(a), (int)(b), __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)

#define TEST_ASSERT_NEQ(a, b) \
    do { \
        if ((a) == (b)) { \
            fprintf(stderr, "ASSERTION FAILED: %s != %s\n  at %s:%d\n", \
                    #a, #b, __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)

#define TEST_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            fprintf(stderr, "ASSERTION FAILED: %s is NULL\n  at %s:%d\n", \
                    #ptr, __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)

#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            fprintf(stderr, "ASSERTION FAILED: %s is not NULL\n  at %s:%d\n", \
                    #ptr, __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)

#define TEST_ASSERT_STR_EQ(a, b) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            fprintf(stderr, "ASSERTION FAILED: %s == %s (\"%s\" != \"%s\")\n  at %s:%d\n", \
                    #a, #b, (a), (b), __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)

#define TEST_ASSERT_MEM_EQ(a, b, len) \
    do { \
        if (memcmp((a), (b), (len)) != 0) { \
            fprintf(stderr, "ASSERTION FAILED: memcmp(%s, %s, %zu) == 0\n  at %s:%d\n", \
                    #a, #b, (size_t)(len), __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)

#define TEST_SUCCESS(rc) \
    do { \
        if ((rc) != 0) { \
            fprintf(stderr, "OPERATION FAILED: %s returned %d (expected 0)\n  at %s:%d\n", \
                    #rc, (rc), __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)

#define TEST_FAILURE(rc) \
    do { \
        if ((rc) == 0) { \
            fprintf(stderr, "OPERATION SHOULD HAVE FAILED: %s returned 0\n  at %s:%d\n", \
                    #rc, __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)

/* Test execution macro */
#define RUN_TEST(test_func) \
    do { \
        printf("Running %s...\n", #test_func); \
        test_func(); \
        printf("  PASSED\n"); \
    } while (0)

/* Helper functions */
static inline void test_sleep_ms(int ms)
{
    slk_sleep(ms);
}

static inline uint64_t test_clock_ms()
{
    return slk_clock() / 1000;
}

/* Context helper - creates a new context */
static inline slk_ctx_t* test_context_new()
{
    slk_ctx_t *ctx = slk_ctx_new();
    TEST_ASSERT_NOT_NULL(ctx);
    return ctx;
}

/* Context helper - destroys a context */
static inline void test_context_destroy(slk_ctx_t *ctx)
{
    if (ctx) {
        slk_ctx_destroy(ctx);
    }
}

/* Socket helper - creates a socket */
static inline slk_socket_t* test_socket_new(slk_ctx_t *ctx, int type)
{
    slk_socket_t *s = slk_socket(ctx, type);
    TEST_ASSERT_NOT_NULL(s);
    return s;
}

/* Socket helper - closes a socket */
static inline void test_socket_close(slk_socket_t *s)
{
    if (s) {
        int rc = slk_close(s);
        TEST_SUCCESS(rc);
    }
}

/* Socket helper - bind to endpoint with retry for TCP port conflicts */
static inline void test_socket_bind(slk_socket_t *s, const char *endpoint)
{
    int rc = slk_bind(s, endpoint);

    /* If bind fails on TCP, try alternative strategies */
    if (rc != 0 && strncmp(endpoint, "tcp://", 6) == 0) {
        int err = slk_errno();
        /* EACCES (13, 10013 Windows), EADDRINUSE (98 Linux, 10048 Windows) */
        if (err == 13 || err == 98 || err == 10048 || err == 10013) {
            /* On permission denied (EACCES), try different ports
             * On address in use, wait and retry same port first */
            int delays[] = {50, 100, 200, 500, 1000};  /* Progressive backoff */
            for (int retry = 0; retry < 5 && rc != 0; retry++) {
                const char *try_endpoint = endpoint;
                /* On EACCES, try a new port instead of retrying same one */
                if (err == 13 || err == 10013) {
                    try_endpoint = test_endpoint_tcp();  /* Get new port */
                    fprintf(stderr, "BIND RETRY %d/5: Permission denied, trying new port '%s'...\n",
                            retry + 1, try_endpoint);
                } else {
                    fprintf(stderr, "BIND RETRY %d/5 for endpoint '%s' (errno=%d), waiting %dms...\n",
                            retry + 1, endpoint, err, delays[retry]);
                    test_sleep_ms(delays[retry]);
                }
                rc = slk_bind(s, try_endpoint);
                if (rc != 0) {
                    err = slk_errno();
                }
            }
        }
    }

    if (rc != 0) {
        fprintf(stderr, "BIND FAILED for endpoint '%s': errno=%d (%s)\n",
                endpoint, slk_errno(), strerror(slk_errno()));
    }
    TEST_SUCCESS(rc);
}

/* Socket helper - connect to endpoint */
static inline void test_socket_connect(slk_socket_t *s, const char *endpoint)
{
    int rc = slk_connect(s, endpoint);
    TEST_SUCCESS(rc);
}

/* Socket helper - set routing ID */
static inline void test_set_routing_id(slk_socket_t *s, const char *id)
{
    int rc = slk_setsockopt(s, SLK_ROUTING_ID, id, strlen(id));
    TEST_SUCCESS(rc);
}

/* Socket helper - set integer option */
static inline void test_set_int_option(slk_socket_t *s, int option, int value)
{
    int rc = slk_setsockopt(s, option, &value, sizeof(value));
    TEST_SUCCESS(rc);
}

/* Socket helper - get integer option */
static inline int test_get_int_option(slk_socket_t *s, int option)
{
    int value = 0;
    size_t len = sizeof(value);
    int rc = slk_getsockopt(s, option, &value, &len);
    TEST_SUCCESS(rc);
    return value;
}

/* Message helper - create new message */
static inline slk_msg_t* test_msg_new()
{
    slk_msg_t *msg = slk_msg_new();
    TEST_ASSERT_NOT_NULL(msg);
    return msg;
}

/* Message helper - create message with data */
static inline slk_msg_t* test_msg_new_data(const void *data, size_t size)
{
    slk_msg_t *msg = slk_msg_new_data(data, size);
    TEST_ASSERT_NOT_NULL(msg);
    return msg;
}

/* Message helper - destroy message */
static inline void test_msg_destroy(slk_msg_t *msg)
{
    if (msg) {
        slk_msg_destroy(msg);
    }
}

/* Message helper - send message */
static inline void test_msg_send(slk_msg_t *msg, slk_socket_t *s, int flags)
{
    int rc = slk_msg_send(msg, s, flags);
    TEST_ASSERT(rc >= 0);
}

/* Message helper - receive message */
static inline int test_msg_recv(slk_msg_t *msg, slk_socket_t *s, int flags)
{
    int rc = slk_msg_recv(msg, s, flags);
    return rc;
}

/* Message helper - send simple string */
static inline void test_send_string(slk_socket_t *s, const char *str, int flags)
{
    int rc = slk_send(s, str, strlen(str), flags);
    TEST_ASSERT(rc >= 0);
}

/* Message helper - receive and verify string */
static inline void test_recv_string(slk_socket_t *s, const char *expected, int flags)
{
    char buffer[256];
    int rc = slk_recv(s, buffer, sizeof(buffer), flags);
    TEST_ASSERT(rc >= 0);
    TEST_ASSERT_EQ((size_t)rc, strlen(expected));
    buffer[rc] = '\0';
    TEST_ASSERT_STR_EQ(buffer, expected);
}

/* Poll helper - wait for socket to be readable */
static inline int test_poll_readable(slk_socket_t *s, long timeout_ms)
{
    slk_pollitem_t items[] = {{s, -1, SLK_POLLIN, 0}};
    int rc = slk_poll(items, 1, timeout_ms);
    if (rc > 0 && (items[0].revents & SLK_POLLIN)) {
        return 1;
    }
    return 0;
}

/* Poll helper - wait for socket to be writable */
static inline int test_poll_writable(slk_socket_t *s, long timeout_ms)
{
    slk_pollitem_t items[] = {{s, -1, SLK_POLLOUT, 0}};
    int rc = slk_poll(items, 1, timeout_ms);
    if (rc > 0 && (items[0].revents & SLK_POLLOUT)) {
        return 1;
    }
    return 0;
}

/* Endpoint helper - generate unique endpoint using ephemeral ports */
/* Returns a static buffer per slot - supports up to 32 concurrent endpoints */
static inline const char* test_endpoint_tcp()
{
    static char endpoints[32][64];
    static int base_port = 0;
    static int call_count = 0;

    if (base_port == 0) {
        int pid = (int)getpid();
        /* Use high-resolution clock for better entropy even within the same second */
        uint64_t clock_val = slk_clock();
        
        /* Mix PID and clock bits to create a more unique base port */
        unsigned int seed = (unsigned int)(clock_val ^ (pid << 16) ^ (pid >> 16));
        srand(seed);
        
        /* Safe range: 20000 - 60000 */
        base_port = 20000 + (rand() % 35000);
    }

    int slot = call_count % 32;
    /* Use a larger prime step to minimize collisions */
    int port = base_port + (call_count * 17);
    call_count++;

    /* Wrap around if we exceed port range */
    if (port > 59900 || call_count >= 300) {
        call_count = 0;
        /* Recalculate base port with new clock value for next cycle */
        base_port = 15000 + (int)(slk_clock() % 44000);
    }

    snprintf(endpoints[slot], sizeof(endpoints[slot]), "tcp://127.0.0.1:%d", port);
    return endpoints[slot];
}

/* Endpoint helper - generate IPC endpoint (for platforms that support it) */
static inline const char* test_endpoint_ipc()
{
#if defined(_WIN32)
    static char endpoint[64];
    static int num = 0;
    snprintf(endpoint, sizeof(endpoint), "ipc://serverlink-test-%d", num++);
    return endpoint;
#else
    static char endpoint[64];
    static int num = 0;
    snprintf(endpoint, sizeof(endpoint), "ipc:///tmp/serverlink-test-%d", num++);
    return endpoint;
#endif
}

/* Sequence testing helpers */
#define SEQ_END ((const char*)-1)

/* Simplified version for common cases */
static inline void s_send_seq_2(slk_socket_t *socket, const char *data1, const char *data2)
{
    int rc;
    if (data1 == NULL) {
        rc = slk_send(socket, "", 0, SLK_SNDMORE);
    } else {
        rc = slk_send(socket, data1, strlen(data1), SLK_SNDMORE);
    }
    TEST_ASSERT(rc >= 0);

    if (data2 == NULL) {
        rc = slk_send(socket, "", 0, 0);
    } else {
        rc = slk_send(socket, data2, strlen(data2), 0);
    }
    TEST_ASSERT(rc >= 0);
}

static inline void s_send_seq_1(slk_socket_t *socket, const char *data1)
{
    int rc;
    if (data1 == NULL) {
        rc = slk_send(socket, "", 0, 0);
    } else {
        rc = slk_send(socket, data1, strlen(data1), 0);
    }
    TEST_ASSERT(rc >= 0);
}


/* Simplified version for common cases */
static inline void s_recv_seq_2(slk_socket_t *socket, const char *expected1, const char *expected2)
{
    char buffer[256];
    int rc;

    rc = slk_recv(socket, buffer, sizeof(buffer), 0);
    TEST_ASSERT(rc >= 0);
    if (expected1 == NULL) {
        TEST_ASSERT_EQ(rc, 0);
    } else {
        TEST_ASSERT_EQ((size_t)rc, strlen(expected1));
        TEST_ASSERT_MEM_EQ(buffer, expected1, rc);
    }

    rc = slk_recv(socket, buffer, sizeof(buffer), 0);
    TEST_ASSERT(rc >= 0);
    if (expected2 == NULL) {
        TEST_ASSERT_EQ(rc, 0);
    } else {
        TEST_ASSERT_EQ((size_t)rc, strlen(expected2));
        TEST_ASSERT_MEM_EQ(buffer, expected2, rc);
    }
}

static inline void s_recv_seq_1(slk_socket_t *socket, const char *expected1)
{
    char buffer[256];
    int rc = slk_recv(socket, buffer, sizeof(buffer), 0);
    TEST_ASSERT(rc >= 0);
    if (expected1 == NULL) {
        TEST_ASSERT_EQ(rc, 0);
    } else {
        TEST_ASSERT_EQ((size_t)rc, strlen(expected1));
        TEST_ASSERT_MEM_EQ(buffer, expected1, rc);
    }
}

/* Test setup/teardown helpers */
class TestFixture {
public:
    slk_ctx_t *ctx;

    TestFixture() : ctx(nullptr) {
        ctx = test_context_new();
    }

    ~TestFixture() {
        test_context_destroy(ctx);
    }
};

#endif /* SERVERLINK_TESTUTIL_HPP */
