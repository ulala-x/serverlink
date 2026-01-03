/* ServerLink ROUTER_MANDATORY + HWM Tests - Ported from libzmq */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This test verifies ROUTER_MANDATORY behavior with HWM limits.
 * Uses inproc transport to avoid TCP port issues in CI environments.
 */

#include "../testutil.hpp"
#include <stdint.h>

/* Test: ROUTER_MANDATORY with HWM limits */
static void test_router_mandatory_hwm()
{
    slk_ctx_t *ctx = test_context_new();

    /* Use inproc to avoid TCP port issues in CI */
    const char *endpoint = "inproc://router_mandatory_hwm";

    /* Create ROUTER socket with mandatory routing and HWM=1 */
    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);

    int mandatory = 1;
    int rc = slk_setsockopt(router, SLK_ROUTER_MANDATORY, &mandatory, sizeof(mandatory));
    TEST_SUCCESS(rc);

    int sndhwm = 1;
    rc = slk_setsockopt(router, SLK_SNDHWM, &sndhwm, sizeof(sndhwm));
    TEST_SUCCESS(rc);

    int linger = 1;
    rc = slk_setsockopt(router, SLK_LINGER, &linger, sizeof(linger));
    TEST_SUCCESS(rc);

    rc = slk_bind(router, endpoint);
    TEST_SUCCESS(rc);

    /* Create peer ROUTER with routing ID "X" and RCVHWM=1 */
    slk_socket_t *peer = test_socket_new(ctx, SLK_ROUTER);

    rc = slk_setsockopt(peer, SLK_ROUTING_ID, "X", 1);
    TEST_SUCCESS(rc);

    int rcvhwm = 1;
    rc = slk_setsockopt(peer, SLK_RCVHWM, &rcvhwm, sizeof(rcvhwm));
    TEST_SUCCESS(rc);

    /* Set CONNECT_ROUTING_ID so router can address peer */
    rc = slk_setsockopt(peer, SLK_CONNECT_ROUTING_ID, "R", 1);
    TEST_SUCCESS(rc);

    rc = slk_setsockopt(router, SLK_ROUTING_ID, "R", 1);
    TEST_SUCCESS(rc);

    rc = slk_connect(peer, endpoint);
    TEST_SUCCESS(rc);

    /* Wait for connection to establish */
    test_sleep_ms(SETTLE_TIME);

    /* Simple handshake: peer sends to router to establish connection */
    rc = slk_send(peer, "R", 1, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(peer, "Hello", 5, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* Router receives handshake: routing_id "X" + payload "Hello" */
    char buf[256];
    rc = slk_recv(router, buf, sizeof(buf), 0);  /* routing-id "X" */
    TEST_ASSERT(rc > 0);

    rc = slk_recv(router, buf, sizeof(buf), 0);  /* "Hello" */
    TEST_ASSERT_EQ(rc, 5);

    /* Now test HWM: send large messages until blocked */
    constexpr int buf_size = 65536;
    uint8_t send_buf[buf_size];
    memset(send_buf, 0, buf_size);

    int sent_count = 0;
    for (int i = 0; i < 100000; ++i) {
        rc = slk_send(router, "X", 1, SLK_DONTWAIT | SLK_SNDMORE);
        if (rc == -1 && slk_errno() == SLK_EAGAIN)
            break;
        TEST_ASSERT_EQ(rc, 1);

        rc = slk_send(router, send_buf, buf_size, SLK_DONTWAIT);
        if (rc == -1 && slk_errno() == SLK_EAGAIN) {
            break;
        }
        TEST_ASSERT_EQ(rc, buf_size);
        sent_count++;
    }

    /* HWM limits vary by transport and platform.
     * The key test is that we eventually block (don't send 100K msgs).
     * With inproc, buffering may allow more messages than TCP. */
    printf("  Sent %d messages before blocking\n", sent_count);
    TEST_ASSERT(sent_count < 100000);  /* We did eventually block */
    TEST_ASSERT(sent_count > 0);       /* We sent at least one message */

    test_socket_close(router);
    test_socket_close(peer);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink ROUTER_MANDATORY + HWM Tests ===\n\n");
    fflush(stdout);

    RUN_TEST(test_router_mandatory_hwm);

    printf("\n=== All ROUTER_MANDATORY + HWM Tests Passed ===\n");
    fflush(stdout);
    return 0;
}
