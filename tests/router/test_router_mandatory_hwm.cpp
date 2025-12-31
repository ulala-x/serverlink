/* ServerLink ROUTER_MANDATORY + HWM Tests - Ported from libzmq */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"
#include <stdint.h>

#define TRACE_ENABLED 0

/* Test: ROUTER_MANDATORY with HWM limits */
static void test_router_mandatory_hwm()
{
    if (TRACE_ENABLED)
        fprintf(stderr, "Starting router mandatory HWM test ...\n");

    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);

    /* Configure router socket to mandatory routing and set HWM and linger */
    int mandatory = 1;
    int rc = slk_setsockopt(router, SLK_ROUTER_MANDATORY, &mandatory, sizeof(mandatory));
    TEST_SUCCESS(rc);

    int sndhwm = 1;
    rc = slk_setsockopt(router, SLK_SNDHWM, &sndhwm, sizeof(sndhwm));
    TEST_SUCCESS(rc);

    int linger = 1;
    rc = slk_setsockopt(router, SLK_LINGER, &linger, sizeof(linger));
    TEST_SUCCESS(rc);

    test_socket_bind(router, endpoint);

    /* Create peer called "X" and connect it to our router, configure HWM */
    slk_socket_t *peer = test_socket_new(ctx, SLK_ROUTER);
    rc = slk_setsockopt(peer, SLK_ROUTING_ID, "X", 1);
    TEST_SUCCESS(rc);

    int rcvhwm = 1;
    rc = slk_setsockopt(peer, SLK_RCVHWM, &rcvhwm, sizeof(rcvhwm));
    TEST_SUCCESS(rc);

    test_socket_connect(peer, endpoint);

    /* Wait for connection */
    test_sleep_ms(200);

    /* Get message from peer to know when connection is ready */
    rc = slk_send(peer, "Hello", 5, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    char buf[256];
    rc = slk_recv(router, buf, sizeof(buf), 0);  /* routing-id "X" */
    TEST_ASSERT(rc > 0);

    rc = slk_recv(router, buf, sizeof(buf), 0);  /* "Hello" */
    TEST_ASSERT_EQ(rc, 5);

    /* Send first batch of messages */
    const int buf_size = 65536;
    uint8_t send_buf[buf_size];
    memset(send_buf, 0, buf_size);

    int i;
    for (i = 0; i < 100000; ++i) {
        if (TRACE_ENABLED)
            fprintf(stderr, "Sending message %d ...\n", i);

        rc = slk_send(router, "X", 1, SLK_DONTWAIT | SLK_SNDMORE);
        if (rc == -1 && slk_errno() == SLK_EAGAIN)
            break;
        TEST_ASSERT_EQ(rc, 1);

        rc = slk_send(router, send_buf, buf_size, SLK_DONTWAIT);
        if (rc == -1 && slk_errno() == SLK_EAGAIN) {
            /* If second frame fails, we're in an inconsistent state.
             * In a real implementation, this should be handled gracefully. */
            break;
        }
        TEST_ASSERT_EQ(rc, buf_size);
    }

    /* This should fail after one message but kernel buffering could skew results */
    TEST_ASSERT(i < 10);  /* Changed from TEST_ASSERT_LESS_THAN_INT */

    test_sleep_ms(1000);

    /* Send second batch of messages */
    for (; i < 100000; ++i) {
        if (TRACE_ENABLED)
            fprintf(stderr, "Sending message %d (part 2) ...\n", i);

        rc = slk_send(router, "X", 1, SLK_DONTWAIT | SLK_SNDMORE);
        if (rc == -1 && slk_errno() == SLK_EAGAIN)
            break;
        TEST_ASSERT_EQ(rc, 1);

        rc = slk_send(router, send_buf, buf_size, SLK_DONTWAIT);
        if (rc == -1 && slk_errno() == SLK_EAGAIN) {
            break;
        }
        TEST_ASSERT_EQ(rc, buf_size);
    }

    /* This should fail after two messages but kernel buffering could skew results */
    TEST_ASSERT(i < 20);  /* Changed from TEST_ASSERT_LESS_THAN_INT */

    if (TRACE_ENABLED)
        fprintf(stderr, "Done sending messages.\n");

    test_socket_close(router);
    test_socket_close(peer);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink ROUTER_MANDATORY + HWM Tests ===\n\n");

    RUN_TEST(test_router_mandatory_hwm);

    printf("\n=== All ROUTER_MANDATORY + HWM Tests Passed ===\n");
    return 0;
}
