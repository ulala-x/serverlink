/* Test program for ServerLink C API */

#include <serverlink/serverlink.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    printf("ServerLink C API Test\n");
    printf("=====================\n\n");

    /* Test 1: Version Information */
    printf("Test 1: Version Information\n");
    int major, minor, patch;
    slk_version(&major, &minor, &patch);
    printf("  ServerLink version: %d.%d.%d\n", major, minor, patch);
    assert(major == SLK_VERSION_MAJOR);
    assert(minor == SLK_VERSION_MINOR);
    assert(patch == SLK_VERSION_PATCH);
    printf("  PASSED\n\n");

    /* Test 2: Context Creation */
    printf("Test 2: Context Creation\n");
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx != NULL);
    printf("  Context created successfully\n");
    printf("  PASSED\n\n");

    /* Test 3: Socket Creation */
    printf("Test 3: Socket Creation\n");
    slk_socket_t *socket = slk_socket(ctx, SLK_ROUTER);
    assert(socket != NULL);
    printf("  ROUTER socket created successfully\n");
    printf("  PASSED\n\n");

    /* Test 4: Message API */
    printf("Test 4: Message API\n");

    /* Create a new message */
    slk_msg_t *msg = slk_msg_new();
    assert(msg != NULL);
    printf("  Empty message created\n");

    /* Create message with data */
    const char *test_data = "Hello, ServerLink!";
    slk_msg_t *msg_data = slk_msg_new_data(test_data, strlen(test_data));
    assert(msg_data != NULL);
    printf("  Message with data created\n");

    /* Verify message size */
    size_t msg_size = slk_msg_size(msg_data);
    assert(msg_size == strlen(test_data));
    printf("  Message size: %zu bytes\n", msg_size);

    /* Verify message data */
    void *data_ptr = slk_msg_data(msg_data);
    assert(data_ptr != NULL);
    assert(memcmp(data_ptr, test_data, msg_size) == 0);
    printf("  Message data verified\n");

    /* Clean up messages */
    slk_msg_destroy(msg);
    slk_msg_destroy(msg_data);
    printf("  PASSED\n\n");

    /* Test 5: Socket Options */
    printf("Test 5: Socket Options\n");

    /* Set linger option */
    int linger = 1000;
    int rc = slk_setsockopt(socket, SLK_LINGER, &linger, sizeof(linger));
    if (rc != 0) {
        printf("  ERROR: slk_setsockopt failed with rc=%d, errno=%d (%s)\n",
               rc, slk_errno(), slk_strerror(slk_errno()));
    }
    assert(rc == 0);
    printf("  Set linger option: %d ms\n", linger);

    /* Get linger option */
    int linger_out = 0;
    size_t linger_size = sizeof(linger_out);
    rc = slk_getsockopt(socket, SLK_LINGER, &linger_out, &linger_size);
    if (rc != 0) {
        printf("  ERROR: slk_getsockopt failed with rc=%d, errno=%d (%s)\n",
               rc, slk_errno(), slk_strerror(slk_errno()));
    }
    assert(rc == 0);
    assert(linger_out == linger);
    printf("  Get linger option: %d ms\n", linger_out);

    printf("  PASSED\n\n");

    /* Test 6: Error Handling */
    printf("Test 6: Error Handling\n");

    /* Test with NULL pointer */
    rc = slk_bind(NULL, "tcp://127.0.0.1:5555");
    assert(rc == -1);
    int err = slk_errno();
    printf("  NULL socket error: %d (%s)\n", err, slk_strerror(err));
    assert(err == SLK_EINVAL);

    /* Test with NULL endpoint */
    rc = slk_bind(socket, NULL);
    assert(rc == -1);
    err = slk_errno();
    printf("  NULL endpoint error: %d (%s)\n", err, slk_strerror(err));
    assert(err == SLK_EINVAL);

    printf("  PASSED\n\n");

    /* Test 7: Utility Functions */
    printf("Test 7: Utility Functions\n");

    /* Test clock function */
    uint64_t t1 = slk_clock();
    slk_sleep(10); /* Sleep 10ms */
    uint64_t t2 = slk_clock();
    assert(t2 > t1);
    printf("  Clock test: %lu us elapsed\n", (unsigned long)(t2 - t1));

    printf("  PASSED\n\n");

    /* Cleanup */
    printf("Cleanup:\n");
    rc = slk_close(socket);
    assert(rc == 0);
    printf("  Socket closed\n");

    slk_ctx_destroy(ctx);
    printf("  Context destroyed\n");

    printf("\n======================\n");
    printf("All tests PASSED!\n");
    printf("======================\n");

    return 0;
}
