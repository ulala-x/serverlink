/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - Simplified for ROUTER socket only */

#include "../precompiled.hpp"
#include "../util/macros.hpp"

#include <limits.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#endif

#include <new>
#include <sstream>

#include "stream_engine_base.hpp"
#include "../io/io_thread.hpp"
#include "../core/session_base.hpp"
#include "../auth/mechanism.hpp"
#include "../util/err.hpp"
#include "../util/likely.hpp"
#include "../protocol/wire.hpp"

// Helper to get peer address (simplified)
static std::string get_peer_address (slk::fd_t s_)
{
    std::string peer_address;
    // In a full implementation, would call get_peer_ip_address
    // For now, return empty string
    (void)s_;
    return peer_address;
}

// Helper to unblock socket
static void unblock_socket (slk::fd_t s_)
{
#ifdef _WIN32
    u_long nonblock = 1;
    int rc = ioctlsocket (s_, FIONBIO, &nonblock);
    wsa_assert (rc != SOCKET_ERROR);
#else
    int flags = fcntl (s_, F_GETFL, 0);
    if (flags == -1)
        flags = 0;
    int rc = fcntl (s_, F_SETFL, flags | O_NONBLOCK);
    errno_assert (rc != -1);
#endif
}

slk::stream_engine_base_t::stream_engine_base_t (
  fd_t fd_,
  const options_t &options_,
  const endpoint_uri_pair_t &endpoint_uri_pair_,
  bool has_handshake_stage_) :
    _options (options_),
    _inpos (NULL),
    _insize (0),
    _decoder (NULL),
    _outpos (NULL),
    _outsize (0),
    _encoder (NULL),
    _mechanism (NULL),
    _next_msg (NULL),
    _process_msg (NULL),
    _metadata (NULL),
    _input_stopped (false),
    _output_stopped (false),
    _endpoint_uri_pair (endpoint_uri_pair_),
    _has_handshake_timer (false),
    _has_ttl_timer (false),
    _has_timeout_timer (false),
    _has_heartbeat_timer (false),
    _peer_address (get_peer_address (fd_)),
    _s (fd_),
    _handle (static_cast<handle_t> (NULL)),
    _plugged (false),
    _handshaking (true),
    _io_error (false),
    _session (NULL),
    _socket (NULL),
    _has_handshake_stage (has_handshake_stage_)
{
    const int rc = _tx_msg.init ();
    errno_assert (rc == 0);

    //  Put the socket into non-blocking mode.
    unblock_socket (_s);
}

slk::stream_engine_base_t::~stream_engine_base_t ()
{
    slk_assert (!_plugged);

    if (_s != retired_fd) {
#ifdef _WIN32
        const int rc = closesocket (_s);
        wsa_assert (rc != SOCKET_ERROR);
#else
        int rc = close (_s);
#if defined(__FreeBSD_kernel__) || defined(__FreeBSD__)
        // FreeBSD may return ECONNRESET on close() under load but this is not an error.
        if (rc == -1 && errno == ECONNRESET)
            rc = 0;
#endif
        errno_assert (rc == 0);
#endif
        _s = retired_fd;
    }

    const int rc = _tx_msg.close ();
    errno_assert (rc == 0);

    //  Drop reference to metadata and destroy it if we are the only user.
    if (_metadata != NULL) {
        if (_metadata->drop_ref ()) {
            SL_DELETE (_metadata);
        }
    }

    SL_DELETE (_encoder);
    SL_DELETE (_decoder);
    SL_DELETE (_mechanism);
}

void slk::stream_engine_base_t::plug (io_thread_t *io_thread_,
                                      session_base_t *session_)
{
    slk_assert (!_plugged);
    _plugged = true;

    //  Connect to session object.
    slk_assert (!_session);
    slk_assert (session_);
    _session = session_;
    _socket = _session->get_socket ();

    //  Connect to I/O threads poller object.
    io_object_t::plug (io_thread_);
    _handle = add_fd (_s);

    //  Internal plugging.
    plug_internal ();
}

void slk::stream_engine_base_t::unplug ()
{
    slk_assert (_plugged);
    _plugged = false;

    //  Cancel all timers.
    if (_has_handshake_timer) {
        cancel_timer (handshake_timer_id);
        _has_handshake_timer = false;
    }

    if (_has_ttl_timer) {
        cancel_timer (heartbeat_ttl_timer_id);
        _has_ttl_timer = false;
    }

    if (_has_timeout_timer) {
        cancel_timer (heartbeat_timeout_timer_id);
        _has_timeout_timer = false;
    }

    if (_has_heartbeat_timer) {
        cancel_timer (heartbeat_ivl_timer_id);
        _has_heartbeat_timer = false;
    }

    //  Cancel all fd subscriptions.
    if (!_io_error)
        rm_fd (_handle);

    //  Disconnect from I/O threads poller object.
    io_object_t::unplug ();

    _session = NULL;
}

void slk::stream_engine_base_t::terminate ()
{
    unplug ();
    delete this;
}

void slk::stream_engine_base_t::in_event ()
{
    //  If still handshaking, receive and process the greeting message.
    if (unlikely (_handshaking))
        if (!handshake ())
            return;

    slk_assert (_decoder);

    //  If there has been an I/O error, stop polling.
    if (_input_stopped) {
        rm_fd (_handle);
        _io_error = true;
        return;
    }

    //  If there's no data to process in the buffer...
    if (!_insize) {
        //  Retrieve the buffer and read as much data as possible.
        _decoder->get_buffer (&_inpos, &_insize);
        const int rc = read (_inpos, _insize);

        if (rc == 0) {
            error (connection_error);
            return;
        }
        if (rc == -1) {
            if (errno != EAGAIN)
                error (connection_error);
            return;
        }

        //  Adjust input size
        _insize = static_cast<size_t> (rc);
    }

    int rc = 0;
    size_t processed = 0;

    while (_insize > 0) {
        rc = _decoder->decode (_inpos, _insize, processed);
        slk_assert (processed <= _insize);
        _inpos += processed;
        _insize -= processed;

        if (rc == 0 || rc == -1)
            break;
        rc = (this->*_process_msg) (_decoder->msg ());
        if (rc == -1)
            break;
    }

    //  Tear down the connection if we have failed to decode input data
    //  or the session has rejected the message.
    if (rc == -1) {
        if (errno != EAGAIN) {
            error (protocol_error);
            return;
        }
        _input_stopped = true;
        reset_pollin (_handle);
    }

    _session->flush ();
}

void slk::stream_engine_base_t::out_event ()
{
    //  If write buffer is empty, try to read new data from the encoder.
    if (!_outsize) {
        //  Even when we stop polling as soon as there is no
        //  data to send, the poller may invoke out_event one
        //  more time due to 'speculative write' optimisation.
        if (unlikely (_encoder == NULL)) {
            slk_assert (_handshaking);
            return;
        }

        _outpos = NULL;
        _outsize = _encoder->encode (&_outpos, 0);

        while (_outsize < _options.out_batch_size) {
            if ((this->*_next_msg) (&_tx_msg) == -1)
                break;
            _encoder->load_msg (&_tx_msg);
            const unsigned char *bufptr = _outpos;
            const size_t n = _encoder->encode (&_outpos, _options.out_batch_size - _outsize);
            slk_assert (bufptr == _outpos || n == 0);
            if (n == 0)
                break;
            _outsize += n;
        }

        //  If there is no data to send, stop polling for output.
        if (_outsize == 0) {
            _output_stopped = true;
            reset_pollout ();
            return;
        }
    }

    //  If there are any data to write in write buffer, write as much as
    //  possible to the socket. Note that amount of data to write can be
    //  arbitrarily large. However, we assume that underlying TCP layer has
    //  limited transmission buffer and thus the actual number of bytes
    //  written should be reasonably modest.
    const int nbytes = write (_outpos, _outsize);

    //  IO error has occurred. We stop waiting for output events.
    //  The engine is not terminated until we detect input error;
    //  this is necessary to prevent losing incoming messages.
    if (nbytes == -1) {
        reset_pollout ();
        return;
    }

    _outpos += nbytes;
    _outsize -= nbytes;

    //  If we are still handshaking and there are no data
    //  to send, stop polling for output.
    if (unlikely (_handshaking))
        if (_outsize == 0)
            reset_pollout ();
}

void slk::stream_engine_base_t::restart_output ()
{
    if (unlikely (_io_error))
        return;

    if (likely (_output_stopped)) {
        set_pollout ();
        _output_stopped = false;
    }

    //  Speculative write: The assumption is that at the moment new message
    //  was sent by the user the socket is probably available for writing.
    //  Thus we try to write the data to socket avoiding polling for POLLOUT.
    //  Consequently, the latency should be better in request/reply scenarios.
    out_event ();
}

bool slk::stream_engine_base_t::restart_input ()
{
    slk_assert (_input_stopped);
    slk_assert (_session != NULL);
    slk_assert (_decoder != NULL);

    const int rc = (this->*_process_msg) (_decoder->msg ());
    if (rc == -1) {
        if (errno == EAGAIN)
            _session->flush ();
        else
            error (protocol_error);
        return false;
    }

    while (_insize > 0) {
        size_t processed = 0;
        const int rc_decode = _decoder->decode (_inpos, _insize, processed);
        slk_assert (processed <= _insize);
        _inpos += processed;
        _insize -= processed;
        if (rc_decode == 0 || rc_decode == -1)
            break;
        const int rc_process = (this->*_process_msg) (_decoder->msg ());
        if (rc_process == -1) {
            if (errno == EAGAIN)
                _session->flush ();
            else
                error (protocol_error);
            return false;
        }
    }

    set_pollin ();
    _input_stopped = false;

    _session->flush ();

    return true;
}

void slk::stream_engine_base_t::zap_msg_available ()
{
    // ZAP not supported in ServerLink
}

void slk::stream_engine_base_t::timer_event (int id_)
{
    if (id_ == handshake_timer_id) {
        _has_handshake_timer = false;
        //  Handshake timer expired before handshake completed.
        error (timeout_error);
    } else if (id_ == heartbeat_ivl_timer_id) {
        _has_heartbeat_timer = false;
        // Send PING
        if (_has_heartbeat_timer) {
            cancel_timer (heartbeat_ivl_timer_id);
            _has_heartbeat_timer = false;
        }
    } else if (id_ == heartbeat_timeout_timer_id) {
        _has_timeout_timer = false;
        error (timeout_error);
    } else if (id_ == heartbeat_ttl_timer_id) {
        _has_ttl_timer = false;
        error (timeout_error);
    } else
        slk_assert (false);
}

int slk::stream_engine_base_t::read (void *data_, size_t size_)
{
    return tcp_read (_s, data_, size_);
}

int slk::stream_engine_base_t::write (const void *data_, size_t size_)
{
    return tcp_write (_s, data_, size_);
}

void slk::stream_engine_base_t::error (error_reason_t reason_)
{
    if (_options.heartbeat_interval > 0 && reason_ == connection_error)
        reason_ = timeout_error;

    slk_assert (_session);
    // Event notification removed - not needed for simplified ServerLink
    _session->engine_error (_handshaking == false, reason_);
    unplug ();
    delete this;
}

void slk::stream_engine_base_t::set_handshake_timer ()
{
    slk_assert (!_has_handshake_timer);

    if (_options.handshake_ivl > 0) {
        add_timer (_options.handshake_ivl, handshake_timer_id);
        _has_handshake_timer = true;
    }
}

int slk::stream_engine_base_t::next_handshake_command (msg_t *msg_)
{
    slk_assert (_mechanism != NULL);
    return _mechanism->next_handshake_command (msg_);
}

int slk::stream_engine_base_t::process_handshake_command (msg_t *msg_)
{
    slk_assert (_mechanism != NULL);
    return _mechanism->process_handshake_command (msg_);
}

int slk::stream_engine_base_t::pull_msg_from_session (msg_t *msg_)
{
    return _session->pull_msg (msg_);
}

int slk::stream_engine_base_t::push_msg_to_session (msg_t *msg_)
{
    return _session->push_msg (msg_);
}

int slk::stream_engine_base_t::pull_and_encode (msg_t *msg_)
{
    slk_assert (_mechanism != NULL);

    if (_session->pull_msg (msg_) == -1)
        return -1;
    if (_mechanism->encode (msg_) == -1)
        return -1;
    return 0;
}

int slk::stream_engine_base_t::decode_and_push (msg_t *msg_)
{
    slk_assert (_mechanism != NULL);

    if (_mechanism->decode (msg_) == -1)
        return -1;

    if (_metadata)
        msg_->set_metadata (_metadata);

    if (_session->push_msg (msg_) == -1) {
        if (errno == EAGAIN)
            _process_msg = &stream_engine_base_t::push_one_then_decode_and_push;
        return -1;
    }
    return 0;
}

int slk::stream_engine_base_t::push_one_then_decode_and_push (msg_t *msg_)
{
    const int rc = _session->push_msg (msg_);
    if (rc == 0)
        _process_msg = &stream_engine_base_t::decode_and_push;
    return rc;
}

int slk::stream_engine_base_t::write_credential (msg_t *msg_)
{
    slk_assert (msg_->flags () & msg_t::credential);
    const int rc = msg_->close ();
    errno_assert (rc == 0);
    return 0;
}

void slk::stream_engine_base_t::mechanism_ready ()
{
    if (_options.heartbeat_interval > 0) {
        add_timer (_options.heartbeat_interval, heartbeat_ivl_timer_id);
        _has_heartbeat_timer = true;
    }

    if (_metadata)
        if (_metadata->drop_ref ()) {
            SL_DELETE (_metadata);
            _metadata = NULL;
        }

    properties_t properties;
    init_properties (properties);

    //  Add ZMQ properties.
    properties_t::const_iterator it;
    for (it = _mechanism->get_zmtp_properties ().begin ();
         it != _mechanism->get_zmtp_properties ().end (); ++it)
        properties.insert (*it);

    slk_assert (_metadata == NULL);
    if (!properties.empty ())
        _metadata = new (std::nothrow) metadata_t (properties);

    _handshaking = false;

    if (_has_handshake_stage)
        _session->engine_ready ();
}

bool slk::stream_engine_base_t::init_properties (properties_t &properties_)
{
    if (_peer_address.empty ())
        return false;
    properties_.insert (std::make_pair ("Peer-Address", _peer_address));
    return true;
}

const slk::endpoint_uri_pair_t &slk::stream_engine_base_t::get_endpoint () const
{
    return _endpoint_uri_pair;
}
