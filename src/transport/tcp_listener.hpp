/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_TCP_LISTENER_HPP_INCLUDED
#define SERVERLINK_TCP_LISTENER_HPP_INCLUDED

#include <asio.hpp>
#include <asio/ip/tcp.hpp>

#include "tcp_address.hpp"
#include "stream_listener_base.hpp"

namespace slk
{
class tcp_listener_t final : public stream_listener_base_t
{
  public:
    tcp_listener_t (slk::io_thread_t *io_thread_,
                    slk::socket_base_t *socket_,
                    const options_t &options_);

    //  Set address to listen on.
    int set_local_address (const char *addr_);

  private:
    //  Start the accept loop.
    void start_accept();

    //  Handler for a new connection.
    void handle_accept(const asio::error_code& ec, asio::ip::tcp::socket socket);

    //  Close the listening socket.
    void close () override;

    //  Asio acceptor for incoming connections.
    asio::ip::tcp::acceptor _acceptor;

    //  Address to listen on.
    tcp_address_t _address;

    //  Lifetime sentinel for async handlers.
    std::shared_ptr<int> _lifetime_sentinel;

    SL_NON_COPYABLE_NOR_MOVABLE (tcp_listener_t)
};
}

#endif
