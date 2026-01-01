/* Debug test for XPUB manual mode */
#include "../testutil.hpp"
#include <stdio.h>

int main()
{
    printf("=== XPUB Manual Debug Test ===\n");

    slk_ctx_t *ctx = test_context_new();

    /* Create a publisher */
    slk_socket_t *pub = test_socket_new(ctx, SLK_XPUB);
    int manual = 1;
    int rc = slk_setsockopt(pub, SLK_XPUB_MANUAL, &manual, sizeof(manual));
    printf("Set XPUB_MANUAL: rc=%d\n", rc);

    rc = slk_bind(pub, "inproc://test_xpub_manual_debug");
    printf("Bind: rc=%d\n", rc);

    /* Create a subscriber */
    slk_socket_t *sub = test_socket_new(ctx, SLK_XSUB);
    rc = slk_connect(sub, "inproc://test_xpub_manual_debug");
    printf("Connect: rc=%d\n", rc);

    /* Subscribe for A */
    const uint8_t subscription[] = {1, 'A', 0};
    rc = slk_send(sub, subscription, sizeof(subscription), 0);
    printf("Send subscription {1,'A',0}: rc=%d\n", rc);

    /* Receive subscription from XSUB */
    char buf[32];
    rc = slk_recv(pub, buf, sizeof(buf), 0);
    printf("Recv subscription on XPUB: rc=%d", rc);
    if (rc > 0) {
        printf(", data=[");
        for (int i = 0; i < rc; i++) {
            printf("%d", (uint8_t)buf[i]);
            if (i < rc - 1) printf(",");
        }
        printf("]\n");
    } else {
        printf(", errno=%d\n", slk_errno());
    }

    /* Now set manual subscription for B */
    rc = slk_setsockopt(pub, SLK_SUBSCRIBE, "B", 1);
    printf("Manual subscribe to 'B': rc=%d\n", rc);

    /* Check topics count */
    int topics_count = 0;
    size_t opt_len = sizeof(topics_count);
    rc = slk_getsockopt(pub, SLK_TOPICS_COUNT, &topics_count, &opt_len);
    printf("Topics count after manual subscribe: %d (rc=%d)\n", topics_count, rc);

    /* Send message "A" */
    rc = slk_send(pub, "A", 1, 0);
    printf("Send 'A': rc=%d\n", rc);

    /* Send message "B" */
    rc = slk_send(pub, "B", 1, 0);
    printf("Send 'B': rc=%d\n", rc);

    /* Try to receive on XSUB */
    test_sleep_ms(100);  // Wait for message to propagate

    rc = slk_recv(sub, buf, sizeof(buf), SLK_DONTWAIT);
    printf("Recv on XSUB: rc=%d", rc);
    if (rc > 0) {
        printf(", data='%c'\n", buf[0]);
    } else {
        printf(", errno=%d\n", slk_errno());
    }

    /* Clean up */
    test_socket_close(pub);
    test_socket_close(sub);
    test_context_destroy(ctx);

    printf("=== Debug Test Complete ===\n");
    return 0;
}
