/* ServerLink SPOT Debug Test */

#include "../testutil.hpp"
#include <cstdio>

int main()
{
    printf("=== SPOT Debug Test ===\n");
    fflush(stdout);

    printf("1. Creating context...\n");
    fflush(stdout);
    slk_ctx_t *ctx = test_context_new();
    printf("   Context created: %p\n", (void*)ctx);
    fflush(stdout);

    printf("2. Creating SPOT instance...\n");
    fflush(stdout);
    slk_spot_t *spot = slk_spot_new(ctx);
    printf("   SPOT created: %p\n", (void*)spot);
    fflush(stdout);

    printf("3. Creating topic...\n");
    fflush(stdout);
    int rc = slk_spot_topic_create(spot, "test:topic");
    printf("   Topic create result: %d\n", rc);
    fflush(stdout);

    printf("4. Subscribing to topic...\n");
    fflush(stdout);
    rc = slk_spot_subscribe(spot, "test:topic");
    printf("   Subscribe result: %d\n", rc);
    fflush(stdout);

    printf("5. Publishing message...\n");
    fflush(stdout);
    const char *msg = "hello";
    rc = slk_spot_publish(spot, "test:topic", msg, 5);
    printf("   Publish result: %d\n", rc);
    fflush(stdout);

    printf("6. Sleeping 100ms...\n");
    fflush(stdout);
    slk_sleep(100);
    printf("   Sleep done\n");
    fflush(stdout);

    printf("7. Setting receive timeout to 500ms...\n");
    fflush(stdout);
    int timeout_ms = 500;
    rc = slk_spot_setsockopt(spot, SLK_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
    printf("   Setsockopt result: %d\n", rc);
    fflush(stdout);

    printf("8. Trying to receive (should timeout if nothing)...\n");
    fflush(stdout);
    char topic[64], data[256];
    size_t topic_len, data_len;
    rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 0);
    printf("   Recv result: %d (errno=%d)\n", rc, slk_errno());
    fflush(stdout);

    if (rc == 0) {
        topic[topic_len] = '\0';
        data[data_len] = '\0';
        printf("   Topic: %s, Data: %s\n", topic, data);
    } else {
        printf("   No message received (expected or timeout)\n");
    }
    fflush(stdout);

    printf("9. Destroying SPOT...\n");
    fflush(stdout);
    slk_spot_destroy(&spot);
    printf("   SPOT destroyed\n");
    fflush(stdout);

    printf("10. Destroying context...\n");
    fflush(stdout);
    test_context_destroy(ctx);
    printf("    Context destroyed\n");
    fflush(stdout);

    printf("=== SPOT Debug Test COMPLETE ===\n");
    return 0;
}
