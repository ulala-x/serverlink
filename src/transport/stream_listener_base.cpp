/* SPDX-License-Identifier: MPL-2.0 */

#include "precompiled.hpp"
#include "stream_listener_base.hpp"
#include "../core/session_base.hpp"
#include "../core/socket_base.hpp"
// TODO: engines will be implemented in Phase 7
// #include "zmtp_engine.hpp"
// #include "raw_engine.hpp"

#ifndef SL_HAVE_WINDOWS
#include <unistd.h>
#else
#include <winsock2.h>
#endif

slk::stream_listener_base_t::stream_listener_base_t (
  slk::io_thread_t *io_thread_,
  slk::socket_base_t *socket_,
  const slk::options_t &options_) :
    own_t (io_thread_, options_),
    io_object_t (io_thread_),
    _s (retired_fd),
    _handle (static_cast<handle_t> (NULL)),
    _socket (socket_)
{
}

slk::stream_listener_base_t::~stream_listener_base_t ()
{
    slk_assert (_s == retired_fd);
    slk_assert (!_handle);
}

int slk::stream_listener_base_t::get_local_address (std::string &addr_) const
{
    addr_ = get_socket_name (_s, socket_end_local);
    return addr_.empty () ? -1 : 0;
}

void slk::stream_listener_base_t::process_plug ()
{
    //  Start polling for incoming connections.
    _handle = add_fd (_s);
    set_pollin (_handle);
}

void slk::stream_listener_base_t::process_term (int linger_)
{
    rm_fd (_handle);
    _handle = static_cast<handle_t> (NULL);
    close ();
    own_t::process_term (linger_);
}

int slk::stream_listener_base_t::close ()
{
    // TODO this is identical to stream_connector_base_t::close

    slk_assert (_s != retired_fd);
#ifdef SL_HAVE_WINDOWS
    const int rc = closesocket (_s);
    wsa_assert (rc != SOCKET_ERROR);
#else
    const int rc = ::close (_s);
    errno_assert (rc == 0);
#endif
    // TODO: event system will be implemented later
    // _socket->event_closed (make_unconnected_bind_endpoint_pair (_endpoint), _s);
    _s = retired_fd;

    return 0;
}

void slk::stream_listener_base_t::create_engine (fd_t fd_)
{
    // TODO: This will be fully implemented in Phase 7 with engines
    // For now, this is a stub

    // const endpoint_uri_pair_t endpoint_pair (
    //   get_socket_name (fd_, socket_end_local),
    //   get_socket_name (fd_, socket_end_remote), endpoint_type_bind);

    // i_engine *engine;
    // if (options.raw_socket)
    //     engine = new (std::nothrow) raw_engine_t (fd_, options, endpoint_pair);
    // else
    //     engine = new (std::nothrow) zmtp_engine_t (fd_, options, endpoint_pair);
    // alloc_assert (engine);

    //  Choose I/O thread to run connecter in. Given that we are already
    //  running in an I/O thread, there must be at least one available.
    // io_thread_t *io_thread = choose_io_thread (options.affinity);
    // slk_assert (io_thread);

    //  Create and launch a session object.
    // session_base_t *session =
    //   session_base_t::create (io_thread, false, _socket, options, NULL);
    // errno_assert (session);
    // session->inc_seqnum ();
    // launch_child (session);
    // send_attach (session, engine, false);

    // TODO: event system
    // _socket->event_accepted (endpoint_pair, fd_);
}
