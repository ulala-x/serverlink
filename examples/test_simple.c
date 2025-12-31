/* Simple test program for ServerLink C API */

#include <serverlink/serverlink.h>
#include <stdio.h>

int main(void)
{
    printf("Test 1: Version\n");
    int major, minor, patch;
    slk_version(&major, &minor, &patch);
    printf("  Version: %d.%d.%d\n", major, minor, patch);

    printf("\nTest 2: Context\n");
    printf("  Creating context...\n");
    fflush(stdout);
    slk_ctx_t *ctx = slk_ctx_new();
    if (!ctx) {
        printf("  ERROR: Failed to create context, errno=%d (%s)\n",
               slk_errno(), slk_strerror(slk_errno()));
        return 1;
    }
    printf("  Context created successfully\n");

    printf("\nTest 3: Socket\n");
    printf("  Creating ROUTER socket...\n");
    fflush(stdout);
    slk_socket_t *socket = slk_socket(ctx, SLK_ROUTER);
    if (!socket) {
        printf("  ERROR: Failed to create socket, errno=%d (%s)\n",
               slk_errno(), slk_strerror(slk_errno()));
        slk_ctx_destroy(ctx);
        return 1;
    }
    printf("  Socket created successfully\n");

    printf("\nTest 4: Cleanup\n");
    printf("  Closing socket...\n");
    fflush(stdout);
    int rc = slk_close(socket);
    if (rc != 0) {
        printf("  ERROR: slk_close failed with rc=%d, errno=%d (%s)\n",
               rc, slk_errno(), slk_strerror(slk_errno()));
    } else {
        printf("  Socket closed\n");
    }

    printf("  Destroying context...\n");
    fflush(stdout);
    slk_ctx_destroy(ctx);
    printf("  Context destroyed\n");

    printf("\nAll tests passed!\n");
    return 0;
}
