/* SPDX-License-Identifier: MPL-2.0 */

/**
 * Test to verify WSAStartup initialization order on Windows
 *
 * This test verifies that WSAStartup is called before any global
 * constructors that need sockets. This is critical for DLL builds
 * where initialization order is unpredictable.
 *
 * Build as a DLL on Windows to test the DllMain implementation.
 */

#include "../../src/io/ip.hpp"
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

namespace slk_test
{

// Global object that creates a socket during static initialization
// This simulates what happens in ctx_t with _term_mailbox
class GlobalSocketCreator
{
public:
    GlobalSocketCreator ()
    {
#ifdef _WIN32
        std::cout << "GlobalSocketCreator: Creating socket during static init...\n";

        // This will fail with WSANOTINITIALISED if DllMain didn't run
        SOCKET s = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (s == INVALID_SOCKET) {
            int err = WSAGetLastError ();
            std::cerr << "ERROR: socket() failed with error " << err << "\n";
            if (err == WSANOTINITIALISED) {
                std::cerr << "WSANOTINITIALISED - WSAStartup was not called!\n";
            }
            _socket_created = false;
        } else {
            std::cout << "SUCCESS: Socket created during static init (handle: "
                      << s << ")\n";
            closesocket (s);
            _socket_created = true;
        }
#else
        // On POSIX, no special initialization needed
        int s = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s >= 0) {
            close (s);
            _socket_created = true;
        } else {
            _socket_created = false;
        }
#endif
    }

    bool socket_created () const { return _socket_created; }

private:
    bool _socket_created;
};

// Global instance - constructor runs during static initialization
static GlobalSocketCreator g_socket_creator;

} // namespace slk_test

int main ()
{
    std::cout << "\n=== WSAStartup Initialization Order Test ===\n\n";

#ifdef _WIN32
    std::cout << "Platform: Windows\n";
    #ifdef _USRDLL
    std::cout << "Build type: DLL (DllMain should handle initialization)\n";
    #else
    std::cout << "Build type: Static library (static initializer should handle initialization)\n";
    #endif
#else
    std::cout << "Platform: POSIX (no WSAStartup needed)\n";
#endif

    std::cout << "\nGlobal constructor test: ";
    if (slk_test::g_socket_creator.socket_created ()) {
        std::cout << "PASSED\n";
        std::cout << "  Socket was successfully created during static initialization\n";
        std::cout << "  This means WSAStartup was called before global constructors\n";
    } else {
        std::cout << "FAILED\n";
        std::cout << "  Socket creation failed during static initialization\n";
        std::cout << "  This indicates WSAStartup was not called in time\n";
        return 1;
    }

    // Test explicit initialization
    std::cout << "\nExplicit initialization test: ";
    bool init_result = slk::initialize_network ();
    std::cout << (init_result ? "PASSED" : "FAILED") << "\n";

    // Test socket creation after main() starts
    std::cout << "\nRuntime socket creation test: ";
    slk::fd_t s = slk::open_socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s != slk::retired_fd) {
        std::cout << "PASSED\n";
        std::cout << "  Socket created successfully at runtime\n";
#ifdef _WIN32
        closesocket (s);
#else
        close (s);
#endif
    } else {
        std::cout << "FAILED\n";
        std::cout << "  Socket creation failed at runtime\n";
        return 1;
    }

    std::cout << "\n=== All Tests Passed ===\n";
    return 0;
}
