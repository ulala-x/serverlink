/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_STREAM_CONNECTER_BASE_HPP_INCLUDED
#define SERVERLINK_STREAM_CONNECTER_BASE_HPP_INCLUDED

#include <memory>
#include <asio.hpp>
#include <asio/steady_timer.hpp>

#include "../core/own.hpp"
#include "../io/i_async_stream.hpp"

namespace slk
{
class io_thread_t;
class session_base_t;
struct address_t;

class stream_connecter_base_t : public own_t
{
  public:
    //  If 'delayed_start' is true connecter first waits for a while,
    //  then starts connection process.
    stream_connecter_base_t (slk::io_thread_t *io_thread_,
                             slk::session_base_t *session_,
                             const options_t &options_,
                             address_t *addr_,
                             bool delayed_start_);

    ~stream_connecter_base_t () override;

  protected:
    //  Handlers for incoming commands.
    void process_plug () final;
    void process_term (int linger_) override;

    //  Internal function to create the engine after connection was established.
    virtual void create_engine (std::unique_ptr<i_async_stream> stream, const std::string &local_address_);

    //  Internal function to add a reconnect timer
    void add_reconnect_timer ();
    
    //  Handler for the reconnect timer
    void handle_reconnect_timer(const asio::error_code& ec);

    //  Close the connecting socket.
    virtual void close () = 0;

    //  Address to connect to. Owned by session_base_t.
    //  It is non-const since some parts may change during opening.
    address_t *const _addr;

    // String representation of endpoint to connect to
    std::string _endpoint;

    // Socket
    slk::socket_base_t *const _socket;
    
    //  Asio timer for reconnect logic.
    asio::steady_timer _reconnect_timer;

  private:
    //  Internal function to return a reconnect backoff delay.
    //  Will modify the current_reconnect_ivl used for next call
    //  Returns the currently used interval
    int get_new_reconnect_ivl ();

    virtual void start_connecting () = 0;

    //  If true, connecter is waiting a while before trying to connect.
    const bool _delayed_start;

    //  Current reconnect ivl, updated for backoff strategy
    int _current_reconnect_ivl;

    SL_NON_COPYABLE_NOR_MOVABLE (stream_connecter_base_t)

  protected:
    //  Reference to the session we belong to.
    slk::session_base_t *const _session;
};
}

#endif
