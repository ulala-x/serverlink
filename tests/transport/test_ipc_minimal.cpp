/* Minimal IPC test to debug hang */
#include "../testutil.hpp"
#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

/* IPC is only supported on Unix-like systems */
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define HAS_IPC_SUPPORT 1
#else
#define HAS_IPC_SUPPORT 0
#endif

int main()
{
#if !HAS_IPC_SUPPORT
    printf("IPC transport is not supported on this platform.\n");
    printf("Skipping IPC minimal test.\n");
    return 0;
#else
    printf("Step 1: Creating context...\n");
    fflush(stdout);

    slk_ctx_t *ctx = slk_ctx_new();
    if (!ctx) {
        printf("ERROR: Context creation failed\n");
        return 1;
    }
    printf("Step 2: Context created\n");
    fflush(stdout);

    printf("Step 3: Creating socket...\n");
    fflush(stdout);

    slk_socket_t *sock = slk_socket(ctx, SLK_ROUTER);
    if (!sock) {
        printf("ERROR: Socket creation failed\n");
        slk_ctx_destroy(ctx);
        return 1;
    }
    printf("Step 4: Socket created\n");
    fflush(stdout);

    char endpoint[256];
    snprintf(endpoint, sizeof(endpoint), "ipc:///tmp/test_%d.sock", (int)getpid());
    printf("Step 5: Binding to %s...\n", endpoint);
    fflush(stdout);

    int rc = slk_bind(sock, endpoint);
    printf("Step 6: Bind returned %d (errno=%d)\n", rc, slk_errno());
    fflush(stdout);

    printf("Step 7: Closing socket...\n");
    fflush(stdout);

    slk_close(sock);
    printf("Step 8: Socket closed\n");
    fflush(stdout);

    printf("Step 9: Destroying context...\n");
    fflush(stdout);

    slk_ctx_destroy(ctx);
    printf("Step 10: Context destroyed\n");
    fflush(stdout);

    printf("SUCCESS: All steps completed\n");
    return 0;
#endif
}
