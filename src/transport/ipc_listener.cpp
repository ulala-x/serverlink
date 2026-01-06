/* SPDX-License-Identifier: MPL-2.0 */

#include "precompiled.hpp"

#if defined SL_HAVE_IPC

#include <new>
#include <string>
#include <memory>

#include "ipc_listener.hpp"
#include "../io/asio/ipc_stream.hpp"
#include "../io/io_thread.hpp"
#include "../util/config.hpp"
#include "../util/err.hpp"
#include "../core/socket_base.hpp"
#include "address.hpp"

#ifndef _WIN32
#include <unistd.h>
#endif

slk::ipc_listener_t::ipc_listener_t(io_thread_t *io_thread_,
                                     socket_base_t *socket_,
                                     const options_t &options_) :
    stream_listener_base_t(io_thread_, socket_, options_),
    _acceptor(io_thread_->get_io_context()),
    _has_file(false),
    _lifetime_sentinel(std::make_shared<int>(0))
{
}

slk::ipc_listener_t::~ipc_listener_t()
{
}

void slk::ipc_listener_t::close()
{
    if (_acceptor.is_open())
        _acceptor.close();

    // Remove the socket file if we created it
    if (_has_file && !_filename.empty()) {
#ifndef _WIN32
        ::unlink(_filename.c_str());
#endif
        _has_file = false;
    }
}

int slk::ipc_listener_t::set_local_address(const char *addr_)
{
    _filename = addr_;

#ifndef _WIN32
    // Remove any existing socket file
    ::unlink(addr_);
#endif

    // Resolve the address
    int rc = _address.resolve(addr_);
    if (rc != 0)
        return -1;

    try {
        asio::local::stream_protocol::endpoint endpoint(addr_);
        _acceptor.open(endpoint.protocol());
        _acceptor.bind(endpoint);
        _acceptor.listen(_options.backlog);
        _has_file = true;
    } catch (const asio::system_error& e) {
        return -1;
    }

    _endpoint = "ipc://" + _filename;

    start_accept();

    return 0;
}

void slk::ipc_listener_t::start_accept()
{
    std::weak_ptr<int> sentinel = _lifetime_sentinel;
    _acceptor.async_accept(
        [this, sentinel](const asio::error_code& ec, asio::local::stream_protocol::socket socket) {
            if (sentinel.expired()) return;
            handle_accept(ec, std::move(socket));
        });
}

void slk::ipc_listener_t::handle_accept(const asio::error_code& ec, asio::local::stream_protocol::socket socket)
{
    if (ec) {
        if (ec == asio::error::operation_aborted) {
            return;
        }
        return;
    }

    // Create the async stream wrapper for the socket
    auto stream = std::make_unique<ipc_stream_t>(std::move(socket));
    
    // Hand off the new stream to the base class to create the engine
    create_engine(std::move(stream));

    // Continue the accept loop
    start_accept();
}

#endif  // SL_HAVE_IPC