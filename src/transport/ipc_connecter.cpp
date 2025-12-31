/* SPDX-License-Identifier: MPL-2.0 */

#include "precompiled.hpp"

#if defined SL_HAVE_IPC

#include <new>
#include <string>

#include "../util/macros.hpp"
#include "ipc_connecter.hpp"
#include "../io/io_thread.hpp"
#include "../util/err.hpp"
#include "address.hpp"
#include "ipc_address.hpp"
#include "../core/session_base.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

slk::ipc_connecter_t::ipc_connecter_t(class io_thread_t *io_thread_,
                                       class session_base_t *session_,
                                       const options_t &options_,
                                       address_t *addr_,
                                       bool delayed_start_) :
    stream_connecter_base_t(io_thread_, session_, options_, addr_, delayed_start_)
{
    slk_assert(_addr);
    slk_assert(_addr->protocol == protocol_name::ipc);
}

slk::ipc_connecter_t::~ipc_connecter_t()
{
}

void slk::ipc_connecter_t::out_event()
{
    // Remove the handle from poller
    rm_handle();

    const fd_t fd = connect();

    // Handle the error condition by attempting to reconnect
    if (fd == retired_fd) {
        close();
        add_reconnect_timer();
        return;
    }

    // Create the engine
    create_engine(fd, get_socket_name(fd, socket_end_local));
}

void slk::ipc_connecter_t::start_connecting()
{
    // Open the connecting socket
    const int rc = open();

    // Connect may succeed in synchronous manner
    if (rc == 0) {
        _handle = add_fd(_s);
        out_event();
    }

    // Connection establishment may be delayed. Poll for its completion
    else if (rc == -1 && errno == EINPROGRESS) {
        _handle = add_fd(_s);
        set_pollout(_handle);
        // TODO: event system
        // _socket->event_connect_delayed(
        //   make_unconnected_connect_endpoint_pair(_endpoint), slk_errno());
    }

    // Handle any other error condition by eventual reconnect
    else {
        if (_s != retired_fd)
            close();
        add_reconnect_timer();
    }
}

std::string
slk::ipc_connecter_t::get_socket_name(slk::fd_t fd_,
                                       socket_end_t socket_end_) const
{
    return slk::get_socket_name<ipc_address_t>(fd_, socket_end_);
}

int slk::ipc_connecter_t::open()
{
    slk_assert(_s == retired_fd);

    // Resolve the address
    if (_addr->resolved.ipc_addr != NULL) {
        SL_DELETE(_addr->resolved.ipc_addr);
    }

    _addr->resolved.ipc_addr = new (std::nothrow) ipc_address_t();
    alloc_assert(_addr->resolved.ipc_addr);

    int rc = _addr->resolved.ipc_addr->resolve(_addr->address.c_str());
    if (rc != 0) {
        SL_DELETE(_addr->resolved.ipc_addr);
        return -1;
    }

    // Create the socket
    _s = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (_s == -1) {
        SL_DELETE(_addr->resolved.ipc_addr);
        return -1;
    }

    // Set the socket to non-blocking mode so that we get async connect()
    int flags = ::fcntl(_s, F_GETFL, 0);
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
    if (rc == -1) {
        goto error;
    }
#endif

    // Connect to the remote peer
    {
        const ipc_address_t *const ipc_addr = _addr->resolved.ipc_addr;
        rc = ::connect(_s, ipc_addr->addr(), ipc_addr->addrlen());
    }

    // Connect was successful immediately
    if (rc == 0) {
        return 0;
    }

    // Translate error codes indicating asynchronous connect has been
    // launched to a uniform EINPROGRESS
    if (errno == EINTR) {
        errno = EINPROGRESS;
    }

    return -1;

error:
    {
        const int err = errno;
        ::close(_s);
        _s = retired_fd;
        SL_DELETE(_addr->resolved.ipc_addr);
        errno = err;
        return -1;
    }
}

slk::fd_t slk::ipc_connecter_t::connect()
{
    // Async connect has finished. Check whether an error occurred
    int err = 0;
    socklen_t len = sizeof(err);

    const int rc = getsockopt(_s, SOL_SOCKET, SO_ERROR,
                               reinterpret_cast<char *>(&err), &len);

    // Assert if the error was caused by bug
    // Networking problems are OK, no need to assert
    if (rc == -1)
        err = errno;
    if (err != 0) {
        errno = err;
        errno_assert(errno != EBADF && errno != ENOPROTOOPT &&
                     errno != ENOTSOCK && errno != ENOBUFS);
        return retired_fd;
    }

    // Return the newly connected socket
    const fd_t result = _s;
    _s = retired_fd;
    return result;
}

#endif  // SL_HAVE_IPC
