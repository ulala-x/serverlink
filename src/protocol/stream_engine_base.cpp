/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - Simplified for ROUTER socket only */

#include "../precompiled.hpp"
#include "../util/macros.hpp"

#include <limits.h>
#include <string.h>

#include <new>
#include <sstream>

#include "stream_engine_base.hpp"
#include "../io/io_thread.hpp"
#include "../core/session_base.hpp"
#include "../auth/mechanism.hpp"
#include "../util/err.hpp"
#include "../util/likely.hpp"
#include "../protocol/wire.hpp"
#include <asio.hpp>
#include "../io/asio/tcp_stream.hpp"

// TODO: Peer address retrieval needs to be adapted for Asio
static std::string get_peer_address (const slk::options_t &options_) 
{
    std::string peer_address;
    // In a full implementation, would call get_peer_ip_address from Asio socket
    return peer_address;
}


slk::stream_engine_base_t::stream_engine_base_t (
  std::unique_ptr<i_async_stream> stream_,
  const options_t &options_,
  const endpoint_uri_pair_t &endpoint_uri_pair_,
  bool has_handshake_stage_) : 
    _stream (std::move (stream_)),
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
    _output_stopped (true), // Output is stopped until a message is sent
    _endpoint_uri_pair (endpoint_uri_pair_),
    _has_handshake_timer (false),
    _has_ttl_timer (false),
    _has_timeout_timer (false),
    _has_heartbeat_timer (false),
    _peer_address (get_peer_address (options_)),
    _plugged (false),
    _handshaking (true),
    _io_error (false),
    _session (NULL),
    _socket (NULL),
    _has_handshake_stage (has_handshake_stage_),
    _lifetime_sentinel (std::make_shared<int> (0)),
    _is_vectorized (false)
{
    const int rc = _tx_msg.init ();
    errno_assert (rc == 0);
    _out_batch.reserve (16);
}

slk::stream_engine_base_t::~stream_engine_base_t () 
{
    slk_assert (!_plugged);

    // Stream is closed automatically by unique_ptr destructor
    // which will cancel pending async operations.

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

    //  Internal plugging.
    plug_internal ();

    // Kick off the initial read operation.
    start_read();
}

void slk::stream_engine_base_t::unplug ()
{
    slk_assert (_plugged);
    _plugged = false;
    
    // Cancel timers if any (TODO: move to Asio timers)

    // Close the stream, which will cancel any pending async operations.
    if (_stream)
        _stream->close();

    _session = NULL;
}

void slk::stream_engine_base_t::terminate ()
{
    unplug ();
    delete this;
}

void slk::stream_engine_base_t::start_read()
{
    // If input is stopped (e.g. pending processing), don't start a new read.
    if (_input_stopped || _io_error) {
        return;
    }
    
    // Retrieve the buffer for the next read operation.
    if (unlikely (!_decoder)) {
        _inpos = _handshake_buffer;
        _insize = sizeof (_handshake_buffer);
    } else {
        _decoder->get_buffer(&_inpos, &_insize);
    }

    // Initiate an asynchronous read.
    std::weak_ptr<int> sentinel = _lifetime_sentinel;
    _stream->async_read(_inpos, _insize,
        [this, sentinel](size_t bytes_transferred, int error_code) {
            if (sentinel.expired()) return;
            handle_read(bytes_transferred, error_code);
        });
}

void slk::stream_engine_base_t::handle_read(size_t bytes_transferred, int error_code)
{
    if (error_code != 0) {
        if (error_code == asio::error::operation_aborted) {
            return;
        }
        error(connection_error);
        return;
    }

    if (bytes_transferred == 0) {
        // This indicates a clean shutdown by the peer.
        error(connection_error);
        return;
    }

    _insize = bytes_transferred;
    
    //  If still handshaking, process the greeting message.
    if (unlikely (_handshaking)) {
        // The derived class processes the raw buffer.
        std::weak_ptr<int> sentinel = _lifetime_sentinel;
        process_handshake_data(_inpos, _insize);
        
        if (sentinel.expired()) return;

        // If handshake is still not complete, we need more data.
        if (_handshaking) {
            start_read();
            return;
        }
        
        // Handshake is complete. The derived class should have consumed
        // some bytes and updated _insize. We proceed to message processing
        // with the remaining data.
    }

    if (!_decoder) {
        // This can happen if handshake completes but there's no more data
        // in the initial buffer.
        start_read();
        return;
    }

    int rc = 0;
    size_t processed = 0;
    std::weak_ptr<int> sentinel = _lifetime_sentinel;
    while (_insize > 0) {
        rc = _decoder->decode(_inpos, _insize, processed);
        slk_assert (processed <= _insize);
        _inpos += processed;
        _insize -= processed;

        if (rc == 0 || rc == -1) // 0 = need more data, -1 = error
            break;

        rc = (this->*_process_msg) (_decoder->msg());
        if (sentinel.expired()) return;
        if (rc == -1)
            break;
    }

    if (rc == -1) {
        if (errno != EAGAIN) {
            error(protocol_error);
            return;
        }
        _input_stopped = true;
    }

    if (_session)
        _session->flush();

    // If input hasn't been stopped, start the next read immediately.
    if (!_input_stopped) {
        start_read();
    }
}

void slk::stream_engine_base_t::start_write ()
{
    if (unlikely (_io_error) || (!_outsize && _out_batch.empty())) {
        _output_stopped = true;
        return;
    }

    _output_stopped = false;

    std::weak_ptr<int> sentinel = _lifetime_sentinel;
    
    if (_is_vectorized) {
        tcp_stream_t* s = static_cast<tcp_stream_t*>(_stream.get());
        s->async_writev(_out_batch, [this, sentinel](size_t bt, int ec) {
            if (sentinel.expired()) return;
            handle_write(bt, ec);
        });
    } else {
        _stream->async_write (
          _outpos, _outsize,
          [this, sentinel] (size_t bytes_transferred, int error_code) {
              if (sentinel.expired ())
                  return;
              handle_write (bytes_transferred, error_code);
          });
    }
}

void slk::stream_engine_base_t::handle_write(size_t bytes_transferred, int error_code)
{
    if (error_code != 0) {
        if (error_code == asio::error::operation_aborted) {
            return;
        }
        _io_error = true;
        _output_stopped = true;
        return;
    }

    if (!_is_vectorized) {
        _outpos += bytes_transferred;
        _outsize -= bytes_transferred;

        if (_outsize > 0) {
            start_write ();
            return;
        }
    } else {
        _out_batch.clear();
        _is_vectorized = false;
    }

    if (unlikely (_encoder == NULL)) {
        _output_stopped = true;
        return;
    }

    // High-Performance Vectorized Batching
    _outsize = 0;
    _outpos = NULL;
    _out_batch.clear();
    
    std::weak_ptr<int> sentinel = _lifetime_sentinel;
    
    while (_out_batch.size() < 16) {
        if (_encoder->is_empty ()) {
            if ((this->*_next_msg) (&_tx_msg) == -1)
                break;
            if (sentinel.expired ()) return;
            _encoder->load_msg (&_tx_msg);
        }

        unsigned char *ptr = NULL;
        size_t n = _encoder->encode (&ptr, _options.out_batch_size);
        if (n > 0) {
            _out_batch.push_back(asio::buffer(ptr, n));
            _outsize += n;
        }
        
        if (!_encoder->is_empty ())
            break;
    }

    if (!_out_batch.empty()) {
        _is_vectorized = true;
        start_write ();
    } else {
        _output_stopped = true;
    }
}


void slk::stream_engine_base_t::restart_output ()
{
    if (unlikely (_io_error))
        return;

    // If output was stopped, try to start it again.
    if (likely (_output_stopped)) {
        //  If write buffer is empty, try to read new data from the encoder.
        if (!_outsize) {
            if (unlikely (_encoder == NULL)) {
                slk_assert (_handshaking);
                return;
            }

            // Batching: Pull as many messages as possible into the encoder
            _outsize = 0;
            _outpos = NULL;
            while (_outsize < static_cast<size_t>(_options.out_batch_size)) {
                if (_encoder->is_empty()) {
                    if ((this->*_next_msg) (&_tx_msg) == -1)
                        break;
                    _encoder->load_msg (&_tx_msg);
                }
                
                unsigned char* buffer_ptr = _outpos ? (_outpos + _outsize) : NULL;
                size_t n = _encoder->encode(&buffer_ptr, static_cast<size_t>(_options.out_batch_size) - _outsize);
                
                if (!_outpos) _outpos = buffer_ptr;
                _outsize += n;
                
                if (!_encoder->is_empty())
                    break;
            }
        }

        if (_outsize > 0) {
            start_write();
        }
    }
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

    _input_stopped = false;
    _session->flush ();

    // Start reading again
    start_read();

    return true;
}

void slk::stream_engine_base_t::zap_msg_available ()
{
    // ZAP not supported in ServerLink
}

void slk::stream_engine_base_t::error (error_reason_t reason_)
{
    if (_io_error) return; // Avoid re-entry
    _io_error = true;

    if (_options.heartbeat_interval > 0 && reason_ == connection_error)
        reason_ = timeout_error;

    if (_session)
    {
        //  Send disconnect notification if ROUTER_NOTIFY is enabled
        //  Only send if handshake was completed (not in handshaking state)
        if ((_options.router_notify & SL_NOTIFY_DISCONNECT) && !_handshaking) {
            _session->rollback ();
            msg_t disconnect_notification;
            disconnect_notification.init ();
            _session->push_msg (&disconnect_notification);
        }

        _session->engine_error (_handshaking == false, reason_);
    }
    
    unplug ();
    delete this;
}

void slk::stream_engine_base_t::set_handshake_timer ()
{
    slk_assert (!_has_handshake_timer);
    // TODO: Port to Asio timers
    // if (_options.handshake_ivl > 0) {
    //     add_timer (_options.handshake_ivl, handshake_timer_id);
    //     _has_handshake_timer = true;
    // }
}

int slk::stream_engine_base_t::next_handshake_command (msg_t *msg_)
{
    slk_assert (_mechanism != NULL);

    if (_mechanism->status () == mechanism_t::ready) {
        mechanism_ready ();
        return pull_and_encode (msg_);
    }
    if (_mechanism->status () == mechanism_t::error) {
        errno = EPROTO;
        return -1;
    }
    const int rc = _mechanism->next_handshake_command (msg_);
    if (rc == 0)
        msg_->set_flags (msg_t::command);
    return rc;
}

int slk::stream_engine_base_t::process_handshake_command (msg_t *msg_)
{
    slk_assert (_mechanism != NULL);
    const int rc = _mechanism->process_handshake_command (msg_);
    if (rc == 0) {
        if (_mechanism->status () == mechanism_t::ready) {
            mechanism_ready ();
        }
        else if (_mechanism->status () == mechanism_t::error) {
            errno = EPROTO;
            return -1;
        }
        if (_output_stopped)
            restart_output ();
    }
    return rc;
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

    if (_mechanism->decode (msg_) == -1) {
        return -1;
    }

    // Process command messages (SUBSCRIBE, CANCEL, PING, PONG, etc.)
    if (msg_->flags () & msg_t::command) {
        process_command_message (msg_);
    }

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
    slk_assert (_mechanism != NULL);
    slk_assert (_session != NULL);

    const blob_t &credential = _mechanism->get_user_id ();
    if (credential.size () > 0) {
        msg_t msg;
        int rc = msg.init_size (credential.size ());
        slk_assert (rc == 0);
        memcpy (msg.data (), credential.data (), credential.size ());
        msg.set_flags (msg_t::credential);
        rc = _session->push_msg (&msg);
        if (rc == -1) {
            rc = msg.close ();
            errno_assert (rc == 0);
            return -1;
        }
    }
    _process_msg = &stream_engine_base_t::decode_and_push;
    return decode_and_push (msg_);
}

void slk::stream_engine_base_t::mechanism_ready ()
{
    // TODO: Port timers
    // if (_options.heartbeat_interval > 0 && !_has_heartbeat_timer) {
    //     add_timer (_options.heartbeat_interval, heartbeat_ivl_timer_id);
    //     _has_heartbeat_timer = true;
    // }

    //  Notify session that engine is ready - creates the pipe
    //  MUST be called before push_msg
    if (_has_handshake_stage) {
        _session->engine_ready ();
    }

    bool flush_session = false;

    //  Push the peer's routing ID to the session
    //  This is essential for ROUTER sockets to identify peers
    if (_options.recv_routing_id) {
        msg_t routing_id;
        _mechanism->peer_routing_id (&routing_id);
        const int rc = _session->push_msg (&routing_id);
        if (rc == -1 && errno == EAGAIN) {
            // Pipe is shutting down
            return;
        }
        errno_assert (rc == 0);
        flush_session = true;
    }

    //  Send connect notification if ROUTER_NOTIFY is enabled
    if (_options.router_notify & SL_NOTIFY_CONNECT) {
        msg_t connect_notification;
        connect_notification.init ();
        const int rc = _session->push_msg (&connect_notification);
        if (rc == -1 && errno == EAGAIN) {
            // Pipe is shutting down
            return;
        }
        errno_assert (rc == 0);
        flush_session = true;
    }

    if (flush_session)
        _session->flush ();

    //  Set up function pointers for message processing
    _next_msg = &stream_engine_base_t::pull_and_encode;
    _process_msg = &stream_engine_base_t::write_credential;

    //  Compile metadata
    if (_metadata)
        if (_metadata->drop_ref ()) {
            SL_DELETE (_metadata);
            _metadata = NULL;
        }

    properties_t properties;
    init_properties (properties);

    //  Add ZMTP properties
    properties_t::const_iterator it;
    for (it = _mechanism->get_zmtp_properties ().begin ();
         it != _mechanism->get_zmtp_properties ().end (); ++it)
        properties.insert (*it);

    slk_assert (_metadata == NULL);
    if (!properties.empty ())
        _metadata = new (std::nothrow) metadata_t (properties);

    _handshaking = false;
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
