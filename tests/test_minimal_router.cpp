/* Minimal ROUTER-to-ROUTER test to debug the issue */
#include "testutil.hpp"

int main()
{
    printf("=== Minimal ROUTER-to-ROUTER Test ===\n");

    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    /* Create receiver */
    slk_socket_t *receiver = test_socket_new(ctx, SLK_ROUTER);
    int rc = slk_setsockopt(receiver, SLK_ROUTING_ID, "RECV", 4);
    TEST_SUCCESS(rc);
    test_socket_bind(receiver, endpoint);

    /* Create sender */
    slk_socket_t *sender = test_socket_new(ctx, SLK_ROUTER);
    rc = slk_setsockopt(sender, SLK_ROUTING_ID, "SEND", 4);
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(sender, SLK_CONNECT_ROUTING_ID, "RECV", 4);
    TEST_SUCCESS(rc);
    test_socket_connect(sender, endpoint);

    printf("Waiting for connection...\n");
    test_sleep_ms(200);

    /* Sender sends handshake */
    printf("Sender sending handshake...\n");
    rc = slk_send(sender, "RECV", 4, SLK_SNDMORE);
    printf("  Sent routing ID: rc=%d\n", rc);
    TEST_ASSERT(rc >= 0);

    rc = slk_send(sender, "HELLO", 5, 0);
    printf("  Sent payload: rc=%d\n", rc);
    TEST_ASSERT(rc >= 0);

    printf("Waiting for message to arrive...\n");
    test_sleep_ms(100);

    /* Check if receiver has data */
    printf("Polling receiver...\n");
    if (test_poll_readable(receiver, 1000)) {
        printf("  Receiver has data!\n");

        char buf[256];
        rc = slk_recv(receiver, buf, sizeof(buf), 0);
        printf("  Received routing ID: rc=%d, data='%.*s'\n", rc, rc, buf);
        TEST_ASSERT(rc > 0);

        rc = slk_recv(receiver, buf, sizeof(buf), 0);
        printf("  Received payload: rc=%d, data='%.*s'\n", rc, rc, buf);
        TEST_ASSERT_EQ(rc, 5);
        TEST_ASSERT_MEM_EQ(buf, "HELLO", 5);

        printf("SUCCESS!\n");
    } else {
        printf("  ERROR: Receiver has no data (timeout)\n");
        printf("  This indicates messages are not being delivered\n");
        return 1;
    }

    test_socket_close(sender);
    test_socket_close(receiver);
    test_context_destroy(ctx);

    printf("\n=== Test Passed ===\n");
    return 0;
}
