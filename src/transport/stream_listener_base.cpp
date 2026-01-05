/* SPDX-License-Identifier: MPL-2.0 */

#include "precompiled.hpp"
#include "stream_listener_base.hpp"
#include "../core/session_base.hpp"
#include "../core/socket_base.hpp"
#include "../protocol/zmtp_engine.hpp"
#include "../io/io_thread.hpp"

#include <new>

slk::stream_listener_base_t::stream_listener_base_t (
  slk::io_thread_t *io_thread_,
  slk::socket_base_t *socket_,
  const slk::options_t &options_) :
    own_t (io_thread_, options_),
    _socket (socket_),
    _io_thread(io_thread_),
    _options(options_)
{
}

slk::stream_listener_base_t::~stream_listener_base_t ()
{
}

int slk::stream_listener_base_t::get_local_address (std::string &addr_) const
{
    // TODO: This should be updated to get the address from the Asio acceptor
    addr_ = _endpoint;
    return addr_.empty () ? -1 : 0;
}

void slk::stream_listener_base_t::process_plug ()
{
    // The derived class (e.g., tcp_listener_t) is now responsible
    // for starting the accept loop in its set_local_address method.
}

void slk::stream_listener_base_t::process_term (int linger_)
{
    close();
    own_t::process_term (linger_);
}

void slk::stream_listener_base_t::create_engine (std::unique_ptr<i_async_stream> stream)
{
    // TODO: The endpoint retrieval needs to be done within the Asio-specific
    // listener and passed along. For now, using the stored endpoint.
    const endpoint_uri_pair_t endpoint_pair (_endpoint, "pending_remote", endpoint_type_bind);

    //  Create the engine object for this connection.
    i_engine *engine =
      new (std::nothrow) zmtp_engine_t (std::move(stream), _options, endpoint_pair);
    alloc_assert (engine);

    //  Choose I/O thread to run session in. Given that we are already
    //  running in an I/O thread, there must be at least one available.
    io_thread_t *io_thread = choose_io_thread (_options.affinity);
    slk_assert (io_thread);

    //  Create and launch a session object.
    session_base_t *session =
      session_base_t::create (io_thread, false, _socket, _options, NULL);
    errno_assert (session);
    session->inc_seqnum ();
    launch_child (session);
    send_attach (session, engine, false);
}
