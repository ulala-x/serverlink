/* ServerLink SPOT PUB/SUB - Multi-Topic Management Example
 *
 * This example demonstrates advanced SPOT features:
 * - Managing multiple topics dynamically
 * - Pattern-based subscriptions
 * - Topic lifecycle (create/destroy)
 * - High water mark configuration
 * - Topic existence checking
 *
 * Use case: A notification system with multiple categories
 */

#include <serverlink/serverlink.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void print_separator(void)
{
    printf("--------------------------------------------------\n");
}

int main(void)
{
    printf("=== ServerLink SPOT Multi-Topic Management ===\n\n");

    /* Initialize context and SPOT instance */
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx != NULL);

    slk_spot_t *spot = slk_spot_new(ctx);
    assert(spot != NULL);

    /* Configure high water marks
     *
     * HWM controls the maximum number of messages queued.
     * When HWM is reached:
     * - XPUB: blocks or drops messages (depending on XPUB_NODROP option)
     * - XSUB: blocks or drops messages
     */
    printf("Configuring high water marks...\n");
    if (slk_spot_set_hwm(spot, 1000, 1000) < 0) {
        fprintf(stderr, "Failed to set HWM: %s\n", slk_strerror(slk_errno()));
        goto cleanup;
    }
    printf("  ✓ Send HWM: 1000 messages\n");
    printf("  ✓ Recv HWM: 1000 messages\n\n");

    print_separator();

    /* Create multiple topics organized by category */
    printf("Creating notification topics...\n");

    const char *notification_topics[] = {
        "notify:email:user1",
        "notify:email:user2",
        "notify:sms:user1",
        "notify:sms:user2",
        "notify:push:user1",
        "notify:push:user2",
        "notify:system:critical",
        "notify:system:info",
        NULL
    };

    for (int i = 0; notification_topics[i] != NULL; i++) {
        if (slk_spot_topic_create(spot, notification_topics[i]) < 0) {
            fprintf(stderr, "Failed to create topic '%s': %s\n",
                    notification_topics[i], slk_strerror(slk_errno()));
            goto cleanup;
        }
        printf("  ✓ Created: %s\n", notification_topics[i]);
    }
    printf("\n");

    print_separator();

    /* List all registered topics */
    char **topics;
    size_t topic_count;
    if (slk_spot_list_topics(spot, &topics, &topic_count) == 0) {
        printf("All registered topics (%zu):\n", topic_count);
        for (size_t i = 0; i < topic_count; i++) {
            printf("  %zu. %s\n", i + 1, topics[i]);
        }
        slk_spot_list_topics_free(topics, topic_count);
        printf("\n");
    }

    print_separator();

    /* Pattern-based subscription
     *
     * Subscribe to all email notifications using pattern matching.
     * Pattern format: "prefix*" matches all topics starting with "prefix".
     */
    printf("Pattern-based subscription...\n");

    if (slk_spot_subscribe_pattern(spot, "notify:email:*") < 0) {
        fprintf(stderr, "Failed to subscribe to pattern: %s\n",
                slk_strerror(slk_errno()));
        goto cleanup;
    }
    printf("  ✓ Subscribed to pattern: notify:email:*\n");
    printf("    (will receive all email notifications)\n\n");

    /* Also subscribe to critical system notifications */
    if (slk_spot_subscribe(spot, "notify:system:critical") < 0) {
        fprintf(stderr, "Failed to subscribe: %s\n", slk_strerror(slk_errno()));
        goto cleanup;
    }
    printf("  ✓ Subscribed to exact topic: notify:system:critical\n\n");

    print_separator();

    /* Wait for subscriptions to propagate */
    slk_sleep(10);

    /* Publish messages to various topics */
    printf("Publishing notifications...\n");

    struct {
        const char *topic;
        const char *message;
    } notifications[] = {
        {"notify:email:user1", "You have new email from Alice"},
        {"notify:email:user2", "Password reset link sent"},
        {"notify:sms:user1", "Your verification code is 123456"},
        {"notify:push:user1", "New comment on your post"},
        {"notify:system:critical", "Database connection lost!"},
        {"notify:system:info", "System update completed"},
        {NULL, NULL}
    };

    int published_count = 0;
    for (int i = 0; notifications[i].topic != NULL; i++) {
        if (slk_spot_publish(spot, notifications[i].topic,
                            notifications[i].message,
                            strlen(notifications[i].message)) == 0) {
            printf("  ✓ [%s] %s\n", notifications[i].topic, notifications[i].message);
            published_count++;
        }
    }
    printf("\nPublished %d notifications\n\n", published_count);

    print_separator();

    /* Receive messages
     *
     * Expected messages (3):
     * - notify:email:user1 (matches pattern)
     * - notify:email:user2 (matches pattern)
     * - notify:system:critical (exact subscription)
     */
    printf("Receiving filtered notifications...\n");
    printf("(expecting 3 messages: 2 emails + 1 critical)\n\n");

    char recv_topic[256];
    char recv_data[1024];
    size_t topic_len, data_len;
    int received_count = 0;

    for (int attempts = 0; attempts < 50 && received_count < 3; attempts++) {
        int rc = slk_spot_recv(spot, recv_topic, sizeof(recv_topic), &topic_len,
                              recv_data, sizeof(recv_data), &data_len,
                              SLK_DONTWAIT);

        if (rc == 0) {
            recv_topic[topic_len] = '\0';
            recv_data[data_len] = '\0';
            printf("  [%d] %s\n", received_count + 1, recv_topic);
            printf("      → %s\n", recv_data);
            received_count++;
        } else if (slk_errno() == SLK_EAGAIN) {
            slk_sleep(10);
        } else {
            fprintf(stderr, "Receive error: %s\n", slk_strerror(slk_errno()));
            break;
        }
    }

    printf("\n✓ Received %d/%d expected messages\n", received_count, 3);
    printf("  (SMS, push, and info notifications were filtered)\n\n");

    print_separator();

    /* Topic existence checking */
    printf("Checking topic existence...\n");

    const char *check_topics[] = {
        "notify:email:user1",
        "notify:nonexistent:topic",
        NULL
    };

    for (int i = 0; check_topics[i] != NULL; i++) {
        int exists = slk_spot_topic_exists(spot, check_topics[i]);
        if (exists == 1) {
            int is_local = slk_spot_topic_is_local(spot, check_topics[i]);
            printf("  ✓ '%s' exists (%s)\n", check_topics[i],
                   is_local ? "local" : "remote");
        } else if (exists == 0) {
            printf("  ✗ '%s' does not exist\n", check_topics[i]);
        } else {
            fprintf(stderr, "Error checking topic: %s\n", slk_strerror(slk_errno()));
        }
    }
    printf("\n");

    print_separator();

    /* Dynamic topic destruction */
    printf("Destroying email topics...\n");

    const char *destroy_topics[] = {
        "notify:email:user1",
        "notify:email:user2",
        NULL
    };

    for (int i = 0; destroy_topics[i] != NULL; i++) {
        if (slk_spot_topic_destroy(spot, destroy_topics[i]) == 0) {
            printf("  ✓ Destroyed: %s\n", destroy_topics[i]);
        } else {
            fprintf(stderr, "  ✗ Failed to destroy '%s': %s\n",
                    destroy_topics[i], slk_strerror(slk_errno()));
        }
    }
    printf("\n");

    /* Verify topics are gone */
    if (slk_spot_list_topics(spot, &topics, &topic_count) == 0) {
        printf("Remaining topics (%zu):\n", topic_count);
        for (size_t i = 0; i < topic_count; i++) {
            printf("  %zu. %s\n", i + 1, topics[i]);
        }
        slk_spot_list_topics_free(topics, topic_count);
    }

    printf("\n=== Example completed successfully ===\n");

cleanup:
    slk_spot_destroy(&spot);
    slk_ctx_destroy(ctx);

    return 0;
}
