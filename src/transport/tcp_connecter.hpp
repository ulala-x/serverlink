/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_TCP_CONNECTER_HPP_INCLUDED
#define SERVERLINK_TCP_CONNECTER_HPP_INCLUDED

#include <asio.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ts/buffer.hpp>

#include "../util/constants.hpp"
#include "stream_connecter_base.hpp"
#include "../io/asio/asio_context.hpp" // For global io_context

namespace slk
{
class tcp_connecter_t final : public stream_connecter_base_t
{
  public:
    //  If 'delayed_start' is true connecter first waits for a while,
    //  then starts connection process.
    tcp_connecter_t (slk::io_thread_t *io_thread_,
                     slk::session_base_t *session_,
                     const options_t &options_,
                     address_t *addr_,
                     bool delayed_start_);
    ~tcp_connecter_t ();

  protected:
    // Overrides from stream_connecter_base_t
    void start_connecting () override;
    void close () override;

  private:
    // Handlers for async operations
    void handle_resolve(const asio::error_code& ec, const asio::ip::tcp::resolver::results_type& endpoints);
    void handle_connect(const asio::error_code& ec, const asio::ip::tcp::endpoint& endpoint);

    // Asio members
    asio::ip::tcp::socket _socket;
    asio::ip::tcp::resolver _resolver;
    std::string _host;
    std::string _port;

    //  True iff a timer has been started. (No longer needed, Asio handles connect timeout)
    // bool _connect_timer_started; 

    SL_NON_COPYABLE_NOR_MOVABLE (tcp_connecter_t)
};
}

#endif
