/* SPDX-License-Identifier: MPL-2.0 */

/**
 * Test to verify WSAStartup initialization order on Windows
 *
 * This test verifies that WSAStartup is called before any global
 * constructors that need sockets. This is critical for DLL builds
 * where initialization order is unpredictable.
 *
 * The test uses the public serverlink API to verify that socket
 * operations work correctly, which implicitly tests WSAStartup.
 */

#include <serverlink/serverlink.h>
#include <cstdio>
#include <cstring>

// Test that serverlink can create sockets during static initialization
// This simulates what happens in ctx_t with _term_mailbox
namespace slk_test
{

class GlobalContextCreator
{
public:
    GlobalContextCreator ()
    {
        printf ("GlobalContextCreator: Creating context during static init...\n");

        // Creating a context requires signaler which creates sockets
        // If WSAStartup wasn't called, this would fail on Windows
        slk_ctx_t *ctx = slk_ctx_new ();

        if (ctx != nullptr) {
            printf ("SUCCESS: Context created during static init\n");
            _context_created = true;

            // Create and close a socket to fully test
            slk_socket_t *sock = slk_socket (ctx, SLK_PAIR);
            if (sock != nullptr) {
                printf ("SUCCESS: Socket created during static init\n");
                _socket_created = true;
                slk_close (sock);
            } else {
                printf ("ERROR: Socket creation failed during static init\n");
                _socket_created = false;
            }

            slk_ctx_destroy (ctx);
        } else {
            printf ("ERROR: Context creation failed during static init\n");
            _context_created = false;
            _socket_created = false;
        }
    }

    bool context_created () const { return _context_created; }
    bool socket_created () const { return _socket_created; }

private:
    bool _context_created = false;
    bool _socket_created = false;
};

// Global instance - constructor runs during static initialization
static GlobalContextCreator g_context_creator;

} // namespace slk_test

int main ()
{
    printf ("\n=== WSAStartup Initialization Order Test ===\n\n");

#ifdef _WIN32
    printf ("Platform: Windows\n");
#ifdef _USRDLL
    printf ("Build type: DLL (DllMain should handle initialization)\n");
#else
    printf ("Build type: Static/EXE (static initializer handles initialization)\n");
#endif
#else
    printf ("Platform: POSIX (no WSAStartup needed)\n");
#endif

    printf ("\nGlobal constructor test:\n");
    if (slk_test::g_context_creator.context_created ()) {
        printf ("  Context creation: PASSED\n");
    } else {
        printf ("  Context creation: FAILED\n");
        printf ("  This indicates WSAStartup was not called in time\n");
        return 1;
    }

    if (slk_test::g_context_creator.socket_created ()) {
        printf ("  Socket creation: PASSED\n");
    } else {
        printf ("  Socket creation: FAILED\n");
        return 1;
    }

    // Test runtime socket operations
    printf ("\nRuntime socket operations test:\n");

    slk_ctx_t *ctx = slk_ctx_new ();
    if (ctx == nullptr) {
        printf ("  Runtime context creation: FAILED\n");
        return 1;
    }
    printf ("  Runtime context creation: PASSED\n");

    slk_socket_t *sock = slk_socket (ctx, SLK_PAIR);
    if (sock == nullptr) {
        printf ("  Runtime socket creation: FAILED\n");
        slk_ctx_destroy (ctx);
        return 1;
    }
    printf ("  Runtime socket creation: PASSED\n");

    // Test bind to a random port
    int rc = slk_bind (sock, "tcp://127.0.0.1:*");
    if (rc < 0) {
        printf ("  Socket bind: FAILED (rc=%d, errno=%d)\n", rc, slk_errno ());
        slk_close (sock);
        slk_ctx_destroy (ctx);
        return 1;
    }
    printf ("  Socket bind: PASSED\n");

    // Clean up
    slk_close (sock);
    slk_ctx_destroy (ctx);

    printf ("\n=== All Tests Passed ===\n");
    return 0;
}
