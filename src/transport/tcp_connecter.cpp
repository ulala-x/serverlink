/* SPDX-License-Identifier: MPL-2.0 */

#include "precompiled.hpp"
#include <new>
#include <string>
#include <memory>

#include "../util/macros.hpp"
#include "tcp_connecter.hpp"
#include "../io/io_thread.hpp"
#include "../util/err.hpp"
#include "../io/ip.hpp"
#include "tcp.hpp"
#include "address.hpp"
#include "tcp_address.hpp"
#include "../core/session_base.hpp"
#include "../io/asio/tcp_stream.hpp"

slk::tcp_connecter_t::tcp_connecter_t (class io_thread_t *io_thread_,
                                       class session_base_t *session_,
                                       const options_t &options_,
                                       address_t *addr_,
                                       bool delayed_start_) :
    stream_connecter_base_t (
      io_thread_, session_, options_, addr_, delayed_start_),
    _socket(io_thread_->get_io_context()),
    _resolver(io_thread_->get_io_context())
{
    slk_assert (_addr->protocol == protocol_name::tcp);
    
    // Parse address string "tcp://host:port" -> "host", "port"
    // TODO: This parsing logic is duplicated and brittle. Should use address_t more effectively.
    std::string address = _addr->address;
    size_t colon_pos = address.rfind(':');
    if (colon_pos != std::string::npos) {
        _host = address.substr(0, colon_pos);
        _port = address.substr(colon_pos + 1);
        
        // Handle IPv6 brackets
        if (!_host.empty() && _host.front() == '[' && _host.back() == ']') {
            _host = _host.substr(1, _host.length() - 2);
        }
    }
}

slk::tcp_connecter_t::~tcp_connecter_t ()
{
}

void slk::tcp_connecter_t::close()
{
    _resolver.cancel();
    if (_socket.is_open()) {
        _socket.close();
    }
}

void slk::tcp_connecter_t::start_connecting ()
{
    // Start async resolution
    _resolver.async_resolve(
        _host, _port,
        [this](const asio::error_code& ec, const asio::ip::tcp::resolver::results_type& endpoints) {
            handle_resolve(ec, endpoints);
        });
}

void slk::tcp_connecter_t::handle_resolve(const asio::error_code& ec, const asio::ip::tcp::resolver::results_type& endpoints)
{
    if (ec) {
        if (ec != asio::error::operation_aborted) {
            close();
            add_reconnect_timer();
        }
        return;
    }

    // Start async connect
    asio::async_connect(
        _socket, endpoints,
        [this](const asio::error_code& ec, const asio::ip::tcp::endpoint& endpoint) {
            handle_connect(ec, endpoint);
        });
}

void slk::tcp_connecter_t::handle_connect(const asio::error_code& ec, const asio::ip::tcp::endpoint& endpoint)
{
    if (ec) {
        if (ec != asio::error::operation_aborted) {
            close();
            add_reconnect_timer();
        }
        return;
    }

    // Apply socket options
    try {
        _socket.set_option(asio::ip::tcp::no_delay(true));
        // TODO: Apply other options like keep-alive
    } catch (const asio::system_error&) {
        // Ignore errors
    }

    // Create stream and engine
    auto stream = std::make_unique<tcp_stream_t>(std::move(_socket));
    std::string local_addr = endpoint.address().to_string() + ":" + std::to_string(endpoint.port()); // Actually remote addr, but used for endpoint pair
    
    create_engine(std::move(stream), local_addr);
}
