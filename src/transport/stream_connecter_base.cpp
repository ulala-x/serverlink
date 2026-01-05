/* SPDX-License-Identifier: MPL-2.0 */

#include "precompiled.hpp"
#include "stream_connecter_base.hpp"
#include "../core/session_base.hpp"
#include "address.hpp"
#include "../util/random.hpp"
#include "../util/err.hpp"
#include "../protocol/zmtp_engine.hpp"
#include "../io/io_thread.hpp"

#include <limits>
#include <new>

slk::stream_connecter_base_t::stream_connecter_base_t (
  slk::io_thread_t *io_thread_,
  slk::session_base_t *session_,
  const slk::options_t &options_,
  slk::address_t *addr_,
  bool delayed_start_) :
    own_t (io_thread_, options_),
    _addr (addr_),
    _socket (session_->get_socket ()),
    _reconnect_timer(io_thread_->get_io_context()),
    _delayed_start (delayed_start_),
    _current_reconnect_ivl (-1),
    _session (session_)
{
    slk_assert (_addr);
    _addr->to_string (_endpoint);
}

slk::stream_connecter_base_t::~stream_connecter_base_t ()
{
}

void slk::stream_connecter_base_t::process_plug ()
{
    if (_delayed_start)
        add_reconnect_timer ();
    else
        start_connecting ();
}

void slk::stream_connecter_base_t::process_term (int linger_)
{
    _reconnect_timer.cancel();
    close ();
    own_t::process_term (linger_);
}

void slk::stream_connecter_base_t::add_reconnect_timer ()
{
    if (options.reconnect_ivl > 0) {
        const int interval = get_new_reconnect_ivl ();
        
        _reconnect_timer.expires_after(std::chrono::milliseconds(interval));
        _reconnect_timer.async_wait(
            [this](const asio::error_code& ec) {
                handle_reconnect_timer(ec);
            });
    }
}

void slk::stream_connecter_base_t::handle_reconnect_timer(const asio::error_code& ec)
{
    if (ec == asio::error::operation_aborted) {
        return;
    }
    
    start_connecting();
}

int slk::stream_connecter_base_t::get_new_reconnect_ivl ()
{
    if (options.reconnect_ivl_max > 0) {
        int candidate_interval = 0;
        if (_current_reconnect_ivl == -1)
            candidate_interval = options.reconnect_ivl;
        else if (_current_reconnect_ivl > std::numeric_limits<int>::max () / 2)
            candidate_interval = std::numeric_limits<int>::max ();
        else
            candidate_interval = _current_reconnect_ivl * 2;

        if (candidate_interval > options.reconnect_ivl_max)
            _current_reconnect_ivl = options.reconnect_ivl_max;
        else
            _current_reconnect_ivl = candidate_interval;
        return _current_reconnect_ivl;
    } else {
        if (_current_reconnect_ivl == -1)
            _current_reconnect_ivl = options.reconnect_ivl;
        //  The new interval is the base interval + random value.
        const int random_jitter = generate_random () % options.reconnect_ivl;
        const int interval =
          _current_reconnect_ivl
              < std::numeric_limits<int>::max () - random_jitter
            ? _current_reconnect_ivl + random_jitter
            : std::numeric_limits<int>::max ();
        return interval;
    }
}

void slk::stream_connecter_base_t::create_engine (
  std::unique_ptr<i_async_stream> stream, const std::string &local_address_)
{
    const endpoint_uri_pair_t endpoint_pair (local_address_, _endpoint,
                                              endpoint_type_connect);

    //  Create the engine object for this connection.
    i_engine *engine =
      new (std::nothrow) zmtp_engine_t (std::move(stream), options, endpoint_pair);
    alloc_assert (engine);

    //  Attach the engine to the corresponding session object.
    send_attach (_session, engine);

    //  Shut the connecter down.
    terminate ();
}
