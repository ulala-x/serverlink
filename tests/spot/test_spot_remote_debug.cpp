/* ServerLink SPOT Remote Debug Test */

#include "../testutil.hpp"
#include <cstdio>

int main()
{
    printf("=== SPOT Remote Debug Test ===\n");
    fflush(stdout);

    printf("1. Creating context...\n");
    fflush(stdout);
    slk_ctx_t *ctx = test_context_new();
    printf("   Context created: %p\n", (void*)ctx);
    fflush(stdout);

    printf("2. Creating publisher SPOT instance...\n");
    fflush(stdout);
    slk_spot_t *pub = slk_spot_new(ctx);
    printf("   Publisher SPOT created: %p\n", (void*)pub);
    fflush(stdout);

    printf("3. Creating subscriber SPOT instance...\n");
    fflush(stdout);
    slk_spot_t *sub = slk_spot_new(ctx);
    printf("   Subscriber SPOT created: %p\n", (void*)sub);
    fflush(stdout);

    printf("4. Publisher creating topic...\n");
    fflush(stdout);
    int rc = slk_spot_topic_create(pub, "remote:test");
    printf("   Topic create result: %d\n", rc);
    fflush(stdout);

    printf("5. Publisher binding to TCP endpoint...\n");
    fflush(stdout);
    const char *endpoint = test_endpoint_tcp();
    printf("   Endpoint: %s\n", endpoint);
    rc = slk_spot_bind(pub, endpoint);
    printf("   Bind result: %d\n", rc);
    fflush(stdout);

    printf("6. Waiting for bind to settle (100ms)...\n");
    fflush(stdout);
    slk_sleep(100);

    printf("7. Subscriber routing topic to publisher endpoint...\n");
    fflush(stdout);
    rc = slk_spot_topic_route(sub, "remote:test", endpoint);
    printf("   Topic route result: %d (errno=%d)\n", rc, slk_errno());
    fflush(stdout);

    printf("8. Subscriber subscribing to topic...\n");
    fflush(stdout);
    rc = slk_spot_subscribe(sub, "remote:test");
    printf("   Subscribe result: %d (errno=%d)\n", rc, slk_errno());
    fflush(stdout);

    printf("9. Waiting for subscribe to settle (100ms)...\n");
    fflush(stdout);
    slk_sleep(100);

    printf("10. Publisher publishing message...\n");
    fflush(stdout);
    const char *msg = "hello remote";
    rc = slk_spot_publish(pub, "remote:test", msg, strlen(msg));
    printf("   Publish result: %d (errno=%d)\n", rc, slk_errno());
    fflush(stdout);

    printf("11. Waiting for message to propagate (100ms)...\n");
    fflush(stdout);
    slk_sleep(100);

    printf("12. Setting subscriber receive timeout to 500ms...\n");
    fflush(stdout);
    int timeout_ms = 500;
    rc = slk_spot_setsockopt(sub, SLK_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
    printf("   Setsockopt result: %d\n", rc);
    fflush(stdout);

    printf("13. Subscriber trying to receive...\n");
    fflush(stdout);
    char topic[64], data[256];
    size_t topic_len, data_len;
    rc = slk_spot_recv(sub, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 0);
    printf("   Recv result: %d (errno=%d)\n", rc, slk_errno());
    fflush(stdout);

    if (rc == 0) {
        topic[topic_len] = '\0';
        data[data_len] = '\0';
        printf("   SUCCESS! Topic: %s, Data: %s\n", topic, data);
    } else {
        printf("   FAILED! No message received (timeout or error)\n");
    }
    fflush(stdout);

    printf("14. Cleaning up...\n");
    fflush(stdout);
    slk_spot_destroy(&pub);
    slk_spot_destroy(&sub);
    test_context_destroy(ctx);
    printf("   Cleanup complete\n");
    fflush(stdout);

    printf("=== SPOT Remote Debug Test %s ===\n", rc == 0 ? "PASSED" : "FAILED");
    return rc == 0 ? 0 : 1;
}
