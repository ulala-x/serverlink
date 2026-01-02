/* SPDX-License-Identifier: MPL-2.0 */

#include "ip.hpp"
#include "../util/err.hpp"
#include "../util/macros.hpp"
#include "../util/config.hpp"
#include "../transport/address.hpp"
#include <string>

#if !defined _WIN32
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <string.h>
#else
#include "windows.hpp"
#include <io.h>
#endif

#if defined SL_HAVE_EVENTFD
#include <sys/eventfd.h>
#endif

namespace slk
{

// Network initialization status
#ifdef _WIN32
static bool s_network_initialized = false;

// Internal initialization function - can be called from DllMain or static initializer
// Note: Not static because DllMain (outside namespace) needs access
void internal_initialize_network ()
{
    if (s_network_initialized)
        return;

    const WORD version_requested = MAKEWORD (2, 2);
    WSADATA wsa_data;
    const int rc = WSAStartup (version_requested, &wsa_data);
    if (rc == 0 && LOBYTE (wsa_data.wVersion) == 2
        && HIBYTE (wsa_data.wVersion) == 2) {
        s_network_initialized = true;
    }
}

// Static initializer as fallback for static library builds
// For DLL builds, DllMain takes precedence
namespace {
struct WindowsNetworkInit {
    WindowsNetworkInit ()
    {
        internal_initialize_network ();
    }
};
static WindowsNetworkInit s_windows_network_init;
}  // anonymous namespace
#endif

bool initialize_network ()
{
#ifdef _WIN32
    internal_initialize_network ();
    return s_network_initialized;
#else
    return true;
#endif
}

fd_t open_socket (int domain_, int type_, int protocol_)
{
    // Ensure network is initialized (WSAStartup on Windows)
    // initialize_network() is idempotent - checks s_network_initialized internally
    initialize_network ();

    int rc;

    // Set SOCK_CLOEXEC if available
#if defined SOCK_CLOEXEC
    type_ |= SOCK_CLOEXEC;
#endif

#if defined _WIN32 && defined WSA_FLAG_NO_HANDLE_INHERIT
    const fd_t s = WSASocket (domain_, type_, protocol_, NULL, 0,
                              WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
#else
    const fd_t s = socket (domain_, type_, protocol_);
#endif

    if (s == retired_fd) {
#ifdef _WIN32
        errno = wsa_error_to_errno (WSAGetLastError ());
#endif
        return retired_fd;
    }

    make_socket_noninheritable (s);

    // Set SO_NOSIGPIPE if available
    rc = set_nosigpipe (s);
    slk_assert (rc == 0);

    return s;
}

void unblock_socket (fd_t s_)
{
#if defined _WIN32
    u_long nonblock = 1;
    const int rc = ioctlsocket (s_, FIONBIO, &nonblock);
    errno_assert (rc != SOCKET_ERROR);
#else
    int flags = fcntl (s_, F_GETFL, 0);
    if (flags == -1)
        flags = 0;
    int rc = fcntl (s_, F_SETFL, flags | O_NONBLOCK);
    errno_assert (rc != -1);
#endif
}

int set_nosigpipe (fd_t s_)
{
#ifdef SO_NOSIGPIPE
    int set = 1;
    int rc = setsockopt (s_, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof (int));
    if (rc != 0 && errno == EINVAL)
        return -1;
    errno_assert (rc == 0);
#else
    SL_UNUSED (s_);
#endif
    return 0;
}

void make_socket_noninheritable (fd_t sock_)
{
#if defined _WIN32 && !defined _WIN32_WCE
    const BOOL brc = SetHandleInformation (reinterpret_cast<HANDLE> (sock_),
                                           HANDLE_FLAG_INHERIT, 0);
    win_assert (brc);
#elif defined FD_CLOEXEC
    const int rc = fcntl (sock_, F_SETFD, FD_CLOEXEC);
    errno_assert (rc != -1);
#else
    SL_UNUSED (sock_);
#endif
}

void shutdown_network ()
{
#ifdef _WIN32
    const int rc = WSACleanup ();
    wsa_assert (rc != SOCKET_ERROR);
#endif
}

#if defined _WIN32
static void tune_tcp_socket (const SOCKET socket_)
{
    BOOL tcp_nodelay = 1;
    const int rc =
      setsockopt (socket_, IPPROTO_TCP, TCP_NODELAY,
                  reinterpret_cast<char *> (&tcp_nodelay), sizeof tcp_nodelay);
    wsa_assert (rc != SOCKET_ERROR);
}

static int make_fdpair_tcpip (fd_t *r_, fd_t *w_)
{
    *w_ = INVALID_SOCKET;
    *r_ = INVALID_SOCKET;

    // Create listening socket
    SOCKET listener = open_socket (AF_INET, SOCK_STREAM, 0);
    wsa_assert (listener != INVALID_SOCKET);

    // Set SO_REUSEADDR and TCP_NODELAY
    BOOL so_reuseaddr = 1;
    int rc = setsockopt (listener, SOL_SOCKET, SO_REUSEADDR,
                         reinterpret_cast<char *> (&so_reuseaddr),
                         sizeof so_reuseaddr);
    wsa_assert (rc != SOCKET_ERROR);

    tune_tcp_socket (listener);

    // Bind to loopback with ephemeral port
    struct sockaddr_in addr;
    memset (&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    addr.sin_port = htons (signaler_port);

    rc = bind (listener, reinterpret_cast<const struct sockaddr *> (&addr),
               sizeof addr);

    if (rc != SOCKET_ERROR && signaler_port == 0) {
        // Retrieve ephemeral port number
        int addrlen = sizeof addr;
        rc = getsockname (listener, reinterpret_cast<struct sockaddr *> (&addr),
                          &addrlen);
    }

    // Listen for incoming connections
    if (rc != SOCKET_ERROR) {
        rc = listen (listener, 1);
    }

    // Create writer socket
    if (rc != SOCKET_ERROR) {
        *w_ = open_socket (AF_INET, SOCK_STREAM, 0);
        if (*w_ == INVALID_SOCKET)
            rc = SOCKET_ERROR;
    }

    // Connect writer to listener
    if (rc != SOCKET_ERROR) {
        rc = connect (*w_, reinterpret_cast<struct sockaddr *> (&addr),
                      sizeof addr);
    }

    // Accept connection from writer
    if (rc != SOCKET_ERROR) {
        tune_tcp_socket (*w_);
        *r_ = accept (listener, NULL, NULL);
        if (*r_ == INVALID_SOCKET)
            rc = SOCKET_ERROR;
    }

    // Save errno if error occurred
    int saved_errno = 0;
    if (*r_ == INVALID_SOCKET)
        saved_errno = WSAGetLastError ();

    // Close listening socket
    int rc2 = closesocket (listener);
    wsa_assert (rc2 != SOCKET_ERROR);

    if (*r_ != INVALID_SOCKET) {
        make_socket_noninheritable (*r_);
        return 0;
    }

    // Cleanup on error
    if (*w_ != INVALID_SOCKET) {
        closesocket (*w_);
        *w_ = INVALID_SOCKET;
    }
    errno = wsa_error_to_errno (saved_errno);
    return -1;
}
#endif

int make_fdpair (fd_t *r_, fd_t *w_)
{
#if defined SL_HAVE_EVENTFD
    int flags = 0;
#if defined EFD_CLOEXEC
    flags |= EFD_CLOEXEC;
#endif
    fd_t fd = eventfd (0, flags);
    if (fd == -1) {
        errno_assert (errno == ENFILE || errno == EMFILE);
        *w_ = *r_ = -1;
        return -1;
    }
    *w_ = *r_ = fd;
    return 0;

#elif defined _WIN32
    return make_fdpair_tcpip (r_, w_);

#else
    // Use socketpair for POSIX systems
    int sv[2];
    int type = SOCK_STREAM;
#if defined SOCK_CLOEXEC
    type |= SOCK_CLOEXEC;
#endif
    int rc = socketpair (AF_UNIX, type, 0, sv);
    if (rc == -1) {
        errno_assert (errno == ENFILE || errno == EMFILE);
        *w_ = *r_ = -1;
        return -1;
    } else {
        make_socket_noninheritable (sv[0]);
        make_socket_noninheritable (sv[1]);
        *w_ = sv[0];
        *r_ = sv[1];
        return 0;
    }
#endif
}

void enable_ipv4_mapping (fd_t s_)
{
    SL_UNUSED (s_);

#if defined IPV6_V6ONLY && !defined SL_HAVE_OPENBSD \
  && !defined SL_HAVE_DRAGONFLY
#ifdef SL_HAVE_WINDOWS
    DWORD flag = 0;
#else
    int flag = 0;
#endif
    const int rc = setsockopt (s_, IPPROTO_IPV6, IPV6_V6ONLY,
                               reinterpret_cast<char *> (&flag), sizeof (flag));
#ifdef SL_HAVE_WINDOWS
    wsa_assert (rc != SOCKET_ERROR);
#else
    errno_assert (rc == 0);
#endif
#endif
}

void set_ip_type_of_service (fd_t s_, int iptos_)
{
    int rc = setsockopt (s_, IPPROTO_IP, IP_TOS,
                         reinterpret_cast<char *> (&iptos_), sizeof (iptos_));

#ifdef SL_HAVE_WINDOWS
    wsa_assert (rc != SOCKET_ERROR);
#else
    errno_assert (rc == 0);
#endif

    // Windows and Hurd do not support IPV6_TCLASS
#if !defined(SL_HAVE_WINDOWS) && defined(IPV6_TCLASS)
    rc = setsockopt (s_, IPPROTO_IPV6, IPV6_TCLASS,
                     reinterpret_cast<char *> (&iptos_), sizeof (iptos_));

    // If IPv6 is not enabled ENOPROTOOPT will be returned on Linux and
    // EINVAL on OSX
    if (rc == -1) {
        errno_assert (errno == ENOPROTOOPT || errno == EINVAL);
    }
#endif
}

void set_socket_priority (fd_t s_, int priority_)
{
#ifdef SL_HAVE_SO_PRIORITY
    int rc =
      setsockopt (s_, SOL_SOCKET, SO_PRIORITY,
                  reinterpret_cast<char *> (&priority_), sizeof (priority_));
    errno_assert (rc == 0);
#else
    SL_UNUSED (s_);
    SL_UNUSED (priority_);
#endif
}

int bind_to_device (fd_t s_, const std::string &bound_device_)
{
#ifdef SL_HAVE_SO_BINDTODEVICE
    int rc = setsockopt (s_, SOL_SOCKET, SO_BINDTODEVICE,
                         bound_device_.c_str (), bound_device_.length ());
    if (rc != 0) {
        assert_success_or_recoverable (s_, rc);
        return -1;
    }
    return 0;

#else
    SL_UNUSED (s_);
    SL_UNUSED (bound_device_);

    errno = ENOTSUP;
    return -1;
#endif
}

void assert_success_or_recoverable (fd_t s_, int rc_)
{
#ifdef SL_HAVE_WINDOWS
    if (rc_ != SOCKET_ERROR) {
        return;
    }
#else
    if (rc_ != -1) {
        return;
    }
#endif

    // Check whether an error occurred
    int err = 0;
    slk_socklen_t len = sizeof err;

    const int rc = getsockopt (s_, SOL_SOCKET, SO_ERROR,
                               reinterpret_cast<char *> (&err), &len);

    // Assert if the error was caused by ServerLink bug.
    // Networking problems are OK. No need to assert.
#ifdef SL_HAVE_WINDOWS
    slk_assert (rc == 0);
    if (err != 0) {
        wsa_assert (err == WSAECONNREFUSED || err == WSAECONNRESET
                    || err == WSAECONNABORTED || err == WSAEINTR
                    || err == WSAETIMEDOUT || err == WSAEHOSTUNREACH
                    || err == WSAENETUNREACH || err == WSAENETDOWN
                    || err == WSAENETRESET || err == WSAEACCES
                    || err == WSAEINVAL || err == WSAEADDRINUSE);
    }
#else
    // Following code should handle both Berkeley-derived socket
    // implementations and Solaris.
    if (rc == -1)
        err = errno;
    if (err != 0) {
        errno = err;
        errno_assert (errno == ECONNREFUSED || errno == ECONNRESET
                      || errno == ECONNABORTED || errno == EINTR
                      || errno == ETIMEDOUT || errno == EHOSTUNREACH
                      || errno == ENETUNREACH || errno == ENETDOWN
                      || errno == ENETRESET || errno == EINVAL);
    }
#endif
}

} // namespace slk

//  DllMain - Windows DLL entry point for guaranteed initialization
//  This ensures WSAStartup is called when the DLL loads, which is more
//  reliable than static initialization for DLLs since it's called before
//  any global constructors run.
//  Note: SL_BUILDING_DLL is defined by CMake when building shared library
#if defined _WIN32 && defined SL_BUILDING_DLL

extern "C" BOOL WINAPI DllMain (HINSTANCE hinstDLL,
                                 DWORD fdwReason,
                                 LPVOID lpvReserved)
{
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        // Initialize network when DLL is loaded into process
        // This runs BEFORE any global constructors, ensuring WSAStartup
        // is ready when ctx_t constructor creates signaler objects
        slk::internal_initialize_network ();
        break;

    case DLL_PROCESS_DETACH:
        // Cleanup network when DLL is unloaded
        // lpvReserved is non-NULL if process is terminating
        // In that case, skip cleanup as Windows will do it automatically
        if (lpvReserved == NULL) {
            slk::shutdown_network ();
        }
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        // No per-thread initialization needed
        break;
    }

    // Prevent unused parameter warning
    (void) hinstDLL;

    return TRUE;
}

#endif // _WIN32 && SL_BUILDING_DLL
