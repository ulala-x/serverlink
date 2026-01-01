/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Test capability checking */

#include <serverlink/serverlink.h>
#include <cassert>
#include <cstdio>

int main()
{
    printf("Testing capability checking...\n");

    // Test IPC capability
#ifdef __linux__
    // IPC should be available on Linux
    assert(slk_has("ipc") == 1);
    printf("IPC: supported\n");
#else
    // May or may not be supported on other platforms
    int ipc_support = slk_has("ipc");
    printf("IPC: %s\n", ipc_support ? "supported" : "not supported");
#endif

    // Test unsupported capabilities
    assert(slk_has("curve") == 0);
    printf("CURVE: not supported (expected)\n");

    assert(slk_has("gssapi") == 0);
    printf("GSSAPI: not supported (expected)\n");

    assert(slk_has("pgm") == 0);
    printf("PGM: not supported (expected)\n");

    assert(slk_has("tipc") == 0);
    printf("TIPC: not supported (expected)\n");

    assert(slk_has("norm") == 0);
    printf("NORM: not supported (expected)\n");

    assert(slk_has("draft") == 0);
    printf("DRAFT: not supported (expected)\n");

    // Test unknown capability
    assert(slk_has("unknown_capability") == 0);
    printf("Unknown capability: not supported (expected)\n");

    // Test null capability
    assert(slk_has(nullptr) == 0);
    printf("NULL capability: not supported (expected)\n");

    printf("PASSED\n");
    return 0;
}
