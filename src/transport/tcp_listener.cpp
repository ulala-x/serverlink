/* SPDX-License-Identifier: MPL-2.0 */

#include "precompiled.hpp"
#include <new>
#include <memory>

#include "tcp_listener.hpp"
#include "../io/asio/tcp_stream.hpp"
#include "../io/io_thread.hpp"
#include "../util/err.hpp"
#include "tcp.hpp"
#include "../core/socket_base.hpp"

// Ensure we have access to sockaddr_in/sockaddr_in6
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

slk::tcp_listener_t::tcp_listener_t (io_thread_t *io_thread_,
                                     socket_base_t *socket_,
                                     const options_t &options_) :
    stream_listener_base_t (io_thread_, socket_, options_),
    _acceptor(io_thread_->get_io_context())
{
}

void slk::tcp_listener_t::close()
{
    if (_acceptor.is_open())
        _acceptor.close();
}

int slk::tcp_listener_t::set_local_address (const char *addr_)
{
    fprintf(stderr, "DEBUG: tcp_listener_t::set_local_address for %s\n", addr_);
    fflush(stderr);
    const int rc = _address.resolve(addr_, true, _options.ipv6);
    if (rc != 0) {
        fprintf(stderr, "DEBUG: address.resolve failed for %s\n", addr_);
        fflush(stderr);
        errno = EADDRNOTAVAIL;
        return -1;
    }

    unsigned short port = 0;
    if (_address.family() == AF_INET) {
        const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(_address.addr());
        port = ntohs(sin->sin_port);
    } else {
        const sockaddr_in6* sin6 = reinterpret_cast<const sockaddr_in6*>(_address.addr());
        port = ntohs(sin6->sin6_port);
    }

    asio::ip::tcp::endpoint endpoint(_address.family() == AF_INET ? asio::ip::tcp::v4() : asio::ip::tcp::v6(), port);
    
    // Handle wildcard bindings
    if (strcmp(addr_, "*") == 0 || strstr(addr_, "0.0.0.0") || strstr(addr_, "[::]")) {
        if (_address.family() == AF_INET)
            endpoint.address(asio::ip::address_v4::any());
        else
            endpoint.address(asio::ip::address_v6::any());
    }

    try {
        fprintf(stderr, "DEBUG: opening acceptor for port %d\n", port);
        fflush(stderr);
        _acceptor.open(endpoint.protocol());
        _acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
        _acceptor.bind(endpoint);
        _acceptor.listen(_options.backlog);
        fprintf(stderr, "DEBUG: acceptor is now listening\n");
        fflush(stderr);
    } catch (const asio::system_error& e) {
        fprintf(stderr, "DEBUG: acceptor bind/listen failed: %s\n", e.what());
        fflush(stderr);
        errno = EADDRINUSE;
        return -1;
    }
    
    _endpoint = _acceptor.local_endpoint().address().to_string() + ":" + std::to_string(_acceptor.local_endpoint().port());

    fprintf(stderr, "DEBUG: starting async accept\n");
    fflush(stderr);
    start_accept();

    return 0;
}

void slk::tcp_listener_t::start_accept()
{
    _acceptor.async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
            handle_accept(ec, std::move(socket));
        });
}

void slk::tcp_listener_t::handle_accept(const asio::error_code& ec, asio::ip::tcp::socket socket)
{
    if (ec) {
        // Operation aborted means the acceptor was closed, which is normal.
        if (ec == asio::error::operation_aborted) {
            return;
        }
        // TODO: Handle other errors like ENFILE, EMFILE
        return;
    }

    // Apply socket options to the newly accepted socket
    try {
        socket.set_option(asio::ip::tcp::no_delay(true));
        // TODO: Port other options like keep-alive from tune_tcp_socket
    } catch (const asio::system_error&) {
        // Ignore errors, proceed with default options
    }

    // Create the async stream wrapper for the socket
    auto stream = std::make_unique<tcp_stream_t>(std::move(socket));
    
    // Hand off the new stream to the base class to create the engine
    create_engine(std::move(stream));

    // Continue the accept loop
    start_accept();
}
