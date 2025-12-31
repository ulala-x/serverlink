/* SPDX-License-Identifier: MPL-2.0 */

#include "precompiled.hpp"

#if defined SL_HAVE_IPC

#include <new>
#include <string>

#include "ipc_listener.hpp"
#include "../io/io_thread.hpp"
#include "../util/config.hpp"
#include "../util/err.hpp"
#include "../core/socket_base.hpp"
#include "address.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

slk::ipc_listener_t::ipc_listener_t(io_thread_t *io_thread_,
                                     socket_base_t *socket_,
                                     const options_t &options_) :
    stream_listener_base_t(io_thread_, socket_, options_),
    _has_file(false)
{
}

slk::ipc_listener_t::~ipc_listener_t()
{
    slk_assert(_s == retired_fd);
}

void slk::ipc_listener_t::in_event()
{
    const fd_t fd = accept();

    // If connection cannot be accepted due to insufficient
    // resources, just ignore it for now
    if (fd == retired_fd) {
        // TODO: event system
        // _socket->event_accept_failed(
        //   make_unconnected_bind_endpoint_pair(_endpoint), slk_errno());
        return;
    }

    // Create the engine object for this connection
    create_engine(fd);
}

std::string
slk::ipc_listener_t::get_socket_name(slk::fd_t fd_,
                                      socket_end_t socket_end_) const
{
    return slk::get_socket_name<ipc_address_t>(fd_, socket_end_);
}

int slk::ipc_listener_t::set_local_address(const char *addr_)
{
    // Store the filename for cleanup
    _filename = addr_;

    // Remove any existing socket file
    // This is necessary to avoid EADDRINUSE errors
    ::unlink(addr_);

    // Resolve the address
    int rc = _address.resolve(addr_);
    if (rc != 0)
        return -1;

    // Create the socket
    _s = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (_s == -1)
        return -1;

    // Set socket to non-blocking mode
    const int flags = ::fcntl(_s, F_GETFL, 0);
    if (flags == -1) {
        goto error;
    }
    rc = ::fcntl(_s, F_SETFL, flags | O_NONBLOCK);
    if (rc == -1) {
        goto error;
    }

    // Set close-on-exec
#if defined FD_CLOEXEC
    rc = ::fcntl(_s, F_SETFD, FD_CLOEXEC);
    errno_assert(rc != -1);
#endif

    // Bind the socket
    rc = ::bind(_s, _address.addr(), _address.addrlen());
    if (rc != 0)
        goto error;

    // Mark that we created the file
    _has_file = true;

    // Listen for incoming connections
    rc = ::listen(_s, options.backlog);
    if (rc != 0)
        goto error;

    // Store the endpoint for event reporting
    _endpoint = get_socket_name(_s, socket_end_local);

    // TODO: event system
    // _socket->event_listening(make_unconnected_bind_endpoint_pair(_endpoint), _s);

    return 0;

error:
    const int err = errno;
    close();
    errno = err;
    return -1;
}

slk::fd_t slk::ipc_listener_t::accept()
{
    // Accept one connection and deal with different failure modes
    slk_assert(_s != retired_fd);

    // Accept the connection
    const fd_t sock = ::accept(_s, NULL, NULL);

    if (sock == -1) {
        errno_assert(errno == EAGAIN || errno == EWOULDBLOCK ||
                     errno == EINTR || errno == ECONNABORTED ||
                     errno == EPROTO || errno == ENOBUFS ||
                     errno == ENOMEM || errno == EMFILE ||
                     errno == ENFILE);
        return retired_fd;
    }

    // Set socket to non-blocking mode
    const int flags = ::fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        const int rc = ::close(sock);
        errno_assert(rc == 0);
        return retired_fd;
    }

    int rc = ::fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    if (rc == -1) {
        rc = ::close(sock);
        errno_assert(rc == 0);
        return retired_fd;
    }

    // Set close-on-exec
#if defined FD_CLOEXEC
    rc = ::fcntl(sock, F_SETFD, FD_CLOEXEC);
    if (rc == -1) {
        rc = ::close(sock);
        errno_assert(rc == 0);
        return retired_fd;
    }
#endif

    return sock;
}

int slk::ipc_listener_t::close()
{
    slk_assert(_s != retired_fd);

    // Close the socket first
    const int rc = ::close(_s);
    errno_assert(rc == 0);

    // TODO: event system
    // _socket->event_closed(make_unconnected_bind_endpoint_pair(_endpoint), _s);

    _s = retired_fd;

    // Remove the socket file if we created it
    if (_has_file && !_filename.empty()) {
        ::unlink(_filename.c_str());
        _has_file = false;
    }

    return 0;
}

#endif  // SL_HAVE_IPC
