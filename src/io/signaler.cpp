/* SPDX-License-Identifier: MPL-2.0 */

#include "signaler.hpp"
#include "ip.hpp"
#include "polling_util.hpp"
#include "../util/config.hpp"
#include "../util/likely.hpp"
#include "../util/err.hpp"

#ifdef SL_USE_IOCP
#include "iocp.hpp"
#endif

#if defined SL_USE_EPOLL || defined SL_USE_KQUEUE
#include <poll.h>  // epoll/kqueue systems typically also have poll for signaler
#elif defined SL_USE_SELECT || defined _WIN32
#if defined _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#endif
#endif

#if !defined _WIN32
#include <unistd.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

#if defined SL_HAVE_EVENTFD
#include <sys/eventfd.h>
#endif

#include <algorithm>
#include <climits>

namespace slk
{
#if !defined _WIN32
// Helper to sleep for specific number of milliseconds
static int sleep_ms (unsigned int ms_)
{
    if (ms_ == 0)
        return 0;
    return usleep (ms_ * 1000);
}

// Helper to wait on close() for non-blocking sockets
static int close_wait_ms (int fd_, unsigned int max_ms_ = 2000)
{
    unsigned int ms_so_far = 0;
    const unsigned int min_step_ms = 1;
    const unsigned int max_step_ms = 100;
    // Use parentheses around std::min/std::max to avoid Windows min/max macro conflict
    const unsigned int step_ms =
      (std::min) ((std::max) (min_step_ms, max_ms_ / 10), max_step_ms);

    int rc = 0; // do not sleep on first attempt
    do {
        if (rc == -1 && errno == EAGAIN) {
            sleep_ms (step_ms);
            ms_so_far += step_ms;
        }
        rc = close (fd_);
    } while (ms_so_far < max_ms_ && rc == -1 && errno == EAGAIN);

    return rc;
}
#endif

signaler_t::signaler_t ()
{
    fprintf(stderr, "[signaler_t] Constructor starting: this=%p\n", this);

#ifdef SL_USE_IOCP
    _iocp = nullptr;
    fprintf(stderr, "[signaler_t] IOCP mode: _iocp=NULL\n");
#endif

    fprintf(stderr, "[signaler_t] Creating socketpair\n");
    // Create the socketpair for signaling
    if (make_fdpair (&_r, &_w) == 0) {
        fprintf(stderr, "[signaler_t] Socketpair created: r=%llu, w=%llu\n",
                (uint64_t)_r, (uint64_t)_w);
        fprintf(stderr, "[signaler_t] Unblocking sockets\n");
        unblock_socket (_w);
        unblock_socket (_r);
        fprintf(stderr, "[signaler_t] Sockets unblocked\n");
    } else {
        fprintf(stderr, "[signaler_t] make_fdpair FAILED\n");
    }
#ifdef HAVE_FORK
    pid = getpid ();
#endif

    fprintf(stderr, "[signaler_t] Constructor completed: this=%p\n", this);
}

signaler_t::~signaler_t ()
{
#if defined SL_HAVE_EVENTFD
    if (_r == retired_fd)
        return;
    int rc = close_wait_ms (_r);
    errno_assert (rc == 0);
#elif defined _WIN32
    if (_w != retired_fd) {
        const struct linger so_linger = {1, 0};
        int rc = setsockopt (_w, SOL_SOCKET, SO_LINGER,
                             reinterpret_cast<const char *> (&so_linger),
                             sizeof so_linger);
        // Only check shutdown if WSASTARTUP was previously done
        if (rc == 0 || WSAGetLastError () != WSANOTINITIALISED) {
            wsa_assert (rc != SOCKET_ERROR);
            rc = closesocket (_w);
            wsa_assert (rc != SOCKET_ERROR);
            if (_r == retired_fd)
                return;
            rc = closesocket (_r);
            wsa_assert (rc != SOCKET_ERROR);
        }
    }
#else
    if (_w != retired_fd) {
        int rc = close_wait_ms (_w);
        errno_assert (rc == 0);
    }
    if (_r != retired_fd) {
        int rc = close_wait_ms (_r);
        errno_assert (rc == 0);
    }
#endif
}

fd_t signaler_t::get_fd () const
{
    return _r;
}

void signaler_t::send ()
{
#if defined HAVE_FORK
    if (unlikely (pid != getpid ())) {
        return; // do not send anything in forked child context
    }
#endif

#ifdef SL_USE_IOCP
    // If IOCP is set, use PostQueuedCompletionStatus instead of socket signaling
    fprintf(stderr, "[signaler_t::send] this=%p, _iocp=%p\n", this, _iocp);
    if (_iocp) {
        fprintf(stderr, "[signaler_t::send] Using IOCP path - calling send_signal()\n");
        _iocp->send_signal ();
        fprintf(stderr, "[signaler_t::send] send_signal() returned\n");
        return;
    }
    fprintf(stderr, "[signaler_t::send] IOCP not set - using socket path\n");
    // Fall through to socket-based signaling if IOCP not available
#endif

#if defined SL_HAVE_EVENTFD
    const uint64_t inc = 1;
    ssize_t sz = write (_w, &inc, sizeof (inc));
    errno_assert (sz == sizeof (inc));
#elif defined _WIN32
    const char dummy = 0;
    int nbytes;
    do {
        nbytes = ::send (_w, &dummy, sizeof (dummy), 0);
        wsa_assert (nbytes != SOCKET_ERROR);
    } while (nbytes == SOCKET_ERROR);
    slk_assert (nbytes == sizeof (dummy));
#else
    unsigned char dummy = 0;
    while (true) {
        ssize_t nbytes = ::send (_w, &dummy, sizeof (dummy), 0);
        if (unlikely (nbytes == -1 && errno == EINTR))
            continue;
#if defined HAVE_FORK
        if (unlikely (pid != getpid ())) {
            errno = EINTR;
            break;
        }
#endif
        slk_assert (nbytes == sizeof dummy);
        break;
    }
#endif
}

int signaler_t::wait (int timeout_) const
{
#ifdef HAVE_FORK
    if (unlikely (pid != getpid ())) {
        errno = EINTR;
        return -1;
    }
#endif

#if defined SL_USE_EPOLL || defined SL_USE_KQUEUE
    // Use poll() for signaler wait on epoll/kqueue-based systems
    struct pollfd pfd;
    pfd.fd = _r;
    pfd.events = POLLIN;
    const int rc = poll (&pfd, 1, timeout_);
    if (unlikely (rc < 0)) {
        errno_assert (errno == EINTR);
        return -1;
    }
    if (unlikely (rc == 0)) {
        errno = EAGAIN;
        return -1;
    }
#ifdef HAVE_FORK
    if (unlikely (pid != getpid ())) {
        errno = EINTR;
        return -1;
    }
#endif
    slk_assert (rc == 1);
    slk_assert (pfd.revents & POLLIN);
    return 0;

#elif defined SL_USE_SELECT || defined _WIN32

    optimized_fd_set_t fds (1);
    FD_ZERO (fds.get ());
    FD_SET (_r, fds.get ());
    struct timeval timeout;
    if (timeout_ >= 0) {
        timeout.tv_sec = timeout_ / 1000;
        timeout.tv_usec = timeout_ % 1000 * 1000;
    }
#ifdef _WIN32
    int rc =
      select (0, fds.get (), NULL, NULL, timeout_ >= 0 ? &timeout : NULL);
    wsa_assert (rc != SOCKET_ERROR);
#else
    int rc =
      select (_r + 1, fds.get (), NULL, NULL, timeout_ >= 0 ? &timeout : NULL);
    if (unlikely (rc < 0)) {
        errno_assert (errno == EINTR);
        return -1;
    }
#endif
    if (unlikely (rc == 0)) {
        errno = EAGAIN;
        return -1;
    }
    slk_assert (rc == 1);
    return 0;

#else
#error No polling mechanism defined
#endif
}

void signaler_t::recv ()
{
#if defined SL_HAVE_EVENTFD
    uint64_t dummy;
    ssize_t sz = read (_r, &dummy, sizeof (dummy));
    errno_assert (sz == sizeof (dummy));

    // If we accidentally grabbed the next signal(s) along with the current
    // one, return it back to the eventfd object
    if (unlikely (dummy > 1)) {
        const uint64_t inc = dummy - 1;
        ssize_t sz2 = write (_w, &inc, sizeof (inc));
        errno_assert (sz2 == sizeof (inc));
        return;
    }

    slk_assert (dummy == 1);
#else
    unsigned char dummy;
#if defined _WIN32
    const int nbytes =
      ::recv (_r, reinterpret_cast<char *> (&dummy), sizeof (dummy), 0);
    wsa_assert (nbytes != SOCKET_ERROR);
#else
    ssize_t nbytes = ::recv (_r, &dummy, sizeof (dummy), 0);
    errno_assert (nbytes >= 0);
#endif
    slk_assert (nbytes == sizeof (dummy));
    slk_assert (dummy == 0);
#endif
}

int signaler_t::recv_failable ()
{
#if defined SL_HAVE_EVENTFD
    uint64_t dummy;
    ssize_t sz = read (_r, &dummy, sizeof (dummy));
    if (sz == -1) {
        errno_assert (errno == EAGAIN);
        return -1;
    }
    errno_assert (sz == sizeof (dummy));

    // If we accidentally grabbed the next signal(s) along with the current
    // one, return it back to the eventfd object
    if (unlikely (dummy > 1)) {
        const uint64_t inc = dummy - 1;
        ssize_t sz2 = write (_w, &inc, sizeof (inc));
        errno_assert (sz2 == sizeof (inc));
        return 0;
    }

    slk_assert (dummy == 1);

#else
    unsigned char dummy;
#if defined _WIN32
    const int nbytes =
      ::recv (_r, reinterpret_cast<char *> (&dummy), sizeof (dummy), 0);
    if (nbytes == SOCKET_ERROR) {
        const int last_error = WSAGetLastError ();
        if (last_error == WSAEWOULDBLOCK) {
            errno = EAGAIN;
            return -1;
        }
        wsa_assert (last_error == WSAEWOULDBLOCK);
    }
#else
    ssize_t nbytes = ::recv (_r, &dummy, sizeof (dummy), 0);
    if (nbytes == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            errno = EAGAIN;
            return -1;
        }
        errno_assert (errno == EAGAIN || errno == EWOULDBLOCK
                      || errno == EINTR);
    }
#endif
    slk_assert (nbytes == sizeof (dummy));
    slk_assert (dummy == 0);
#endif
    return 0;
}

bool signaler_t::valid () const
{
    return _w != retired_fd;
}

#ifdef HAVE_FORK
void signaler_t::forked ()
{
    // Close file descriptors created in the parent and create new pair
    close (_r);
    close (_w);
    make_fdpair (&_r, &_w);
}
#endif

#ifdef SL_USE_IOCP
void signaler_t::set_iocp (iocp_t *iocp_)
{
    fprintf(stderr, "[signaler_t::set_iocp] ENTER: this=%p, iocp=%p\n", this, iocp_);
    _iocp = iocp_;
    fprintf(stderr, "[signaler_t::set_iocp] EXIT: _iocp=%p\n", _iocp);
}
#endif

} // namespace slk
