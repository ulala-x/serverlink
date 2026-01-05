/* Context Debug Test */
#include <cstdio>
#include <cstdlib>
#include "../testutil.hpp"

int main()
{
    printf("=== Context Debug Test ===\n");
    fflush(stdout);

    printf("1. About to create context...\n");
    fflush(stdout);

    slk_ctx_t *ctx = slk_ctx_new();

    printf("2. Context created: %p\n", (void*)ctx);
    fflush(stdout);

    if (!ctx) {
        printf("ERROR: Context is NULL!\n");
        return 1;
    }

    printf("3. About to create ROUTER socket...\n");
    fflush(stdout);

    slk_socket_t *sock = slk_socket(ctx, SLK_ROUTER);

    printf("4. Socket created: %p\n", (void*)sock);
    fflush(stdout);

    if (!sock) {
        printf("ERROR: Socket is NULL!\n");
        slk_ctx_destroy(ctx);
        return 1;
    }

    printf("5. About to close socket...\n");
    fflush(stdout);

    int rc = slk_close(sock);

    printf("6. Socket closed (rc=%d)\n", rc);
    fflush(stdout);

    printf("7. About to destroy context...\n");
    fflush(stdout);

    slk_ctx_destroy(ctx);

    printf("8. Context destroyed\n");
    fflush(stdout);

    printf("=== Context Debug Test COMPLETE ===\n");
    return 0;
}
