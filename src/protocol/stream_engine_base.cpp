/* SPDX-License-Identifier: MPL-2.0 */
#include "../precompiled.hpp"
#include <algorithm>
#include "stream_engine_base.hpp"
#include "i_decoder.hpp"
#include "i_encoder.hpp"
#include "../io/io_thread.hpp"
#include "../core/session_base.hpp"
#include "../util/err.hpp"
#include "../util/likely.hpp"
#include "../msg/msg.hpp"

namespace slk {

stream_engine_base_t::stream_engine_base_t (
  std::unique_ptr<i_async_stream> stream_, const options_t &options_,
  const endpoint_uri_pair_t &endpoint_uri_pair_, bool has_handshake_stage_) :
    _options (options_), _plugged (false), _handshaking (true), _has_handshake_timer (false),
    _inpos (NULL), _insize (0), _decoder (NULL), _outpos (NULL), _outsize (0), _encoder (NULL),
    _mechanism (NULL), _next_msg (&stream_engine_base_t::pull_msg_from_session),
    _process_msg (&stream_engine_base_t::decode_and_push), _metadata (NULL),
    _input_stopped (false), _output_stopped (true), _endpoint_uri_pair (endpoint_uri_pair_),
    _peer_address (""), _lifetime_sentinel (std::make_shared<int> (0)),
    _stream (std::move (stream_)), _io_error (false), _session (NULL),
    _has_handshake_stage (has_handshake_stage_), _is_vectorized(false)
{
    _tx_msg.init ();
    _out_batch.reserve(32);
}

stream_engine_base_t::~stream_engine_base_t () { _tx_msg.close (); }

void stream_engine_base_t::plug (io_thread_t *, session_base_t *session_) {
    _session = session_; _plugged = true; start_read ();
}

void stream_engine_base_t::terminate () { unplug (); }
bool stream_engine_base_t::restart_input () { _input_stopped = false; start_read (); return true; }

void stream_engine_base_t::restart_output () {
    if (unlikely (_io_error)) return;
    if (likely (_output_stopped)) {
        if (!_outsize && _out_batch.empty()) {
            if (unlikely (_encoder == NULL)) return;
            fill_out_batch();
        }
        if (_outsize > 0 || !_out_batch.empty()) start_write();
    }
}

// Mirroring libzmq's aggressive output strategy
void stream_engine_base_t::fill_out_batch () {
    _outsize = 0; _outpos = NULL; _out_batch.clear(); _is_vectorized = false;

    if (_handshaking) {
        while (_outsize < (size_t)_options.out_batch_size) {
            if (_encoder->is_empty()) { 
                if ((this->*_next_msg)(&_tx_msg) == -1) break;
                _encoder->load_msg(&_tx_msg); 
            }
            unsigned char* ptr = _outpos ? (_outpos + _outsize) : NULL;
            size_t n = _encoder->encode(&ptr, (size_t)_options.out_batch_size - _outsize);
            if (!_outpos) _outpos = ptr; 
            _outsize += n;
            if (!_encoder->is_empty()) break;
        }
        return;
    }

    // Aggressive Vectorized Batching
    while (_out_batch.size() < 32) {
        if (_encoder->is_empty()) {
            if ((this->*_next_msg)(&_tx_msg) == -1) break;
            _encoder->load_msg(&_tx_msg);
        }
        unsigned char *ptr = NULL;
        size_t n = _encoder->encode(&ptr, 1024 * 1024); // Request massive chunks
        if (n > 0) {
            _out_batch.push_back(asio::buffer(ptr, n));
            _outsize += n;
        }
        if (!_encoder->is_empty()) break;
    }
    if (!_out_batch.empty()) _is_vectorized = true;
}

void stream_engine_base_t::start_read () {
    if (unlikely (_io_error) || _input_stopped) return;
    std::weak_ptr<int> sentinel = _lifetime_sentinel;
    _stream->async_read(_inpos ? _inpos : _handshake_buffer, _decoder ? 8192 : sizeof(_handshake_buffer),
      [this, sentinel] (size_t bt, int ec) { if (!sentinel.expired()) handle_read(bt, ec); });
}

void stream_engine_base_t::handle_read (size_t bt, int ec) {
    if (ec != 0) { error(connection_error); return; }
    _insize = bt; _inpos = _decoder ? _inpos : _handshake_buffer;
    if (unlikely(_handshaking)) {
        process_handshake_data(_inpos, _insize);
        if (_handshaking) { start_read(); return; }
    }
    if (!_decoder) { start_read(); return; }
    size_t processed = 0;
    while (_insize > 0) {
        int rc = _decoder->decode(_inpos, _insize, processed);
        _inpos += processed; _insize -= processed;
        if (rc == 0 || rc == -1) break;
        if ((this->*_process_msg)(_decoder->msg()) == -1) { _input_stopped = true; break; }
    }
    if (_session) _session->flush();
    if (!_input_stopped) start_read();
}

void stream_engine_base_t::start_write () {
    if (unlikely(_io_error)) return;
    _output_stopped = false;
    std::weak_ptr<int> sentinel = _lifetime_sentinel;
    if (_is_vectorized) {
        _stream->async_writev(_out_batch, [this, sentinel](size_t bt, int ec) {
            if (!sentinel.expired()) handle_write(bt, ec);
        });
    } else {
        _stream->async_write(_outpos, _outsize, [this, sentinel](size_t bt, int ec) {
            if (!sentinel.expired()) handle_write(bt, ec);
        });
    }
}

void stream_engine_base_t::handle_write (size_t, int ec) {
    if (ec != 0) { _io_error = true; _output_stopped = true; return; }
    
    // Exactly mirroring libzmq: Release current batch and immediately try next
    _outpos = NULL; _outsize = 0; _out_batch.clear(); _is_vectorized = false;
    if (unlikely(_encoder == NULL)) { _output_stopped = true; return; }
    
    fill_out_batch();
    if (_outsize > 0 || !_out_batch.empty()) start_write();
    else _output_stopped = true;
}

void stream_engine_base_t::error (error_reason_t r) { if (_session) _session->engine_error(true, r); unplug(); }
void stream_engine_base_t::unplug () { if (_plugged) { _stream->close(); _plugged = false; } }
void stream_engine_base_t::zap_msg_available () {}
const endpoint_uri_pair_t &stream_engine_base_t::get_endpoint () const { return _endpoint_uri_pair; }
void stream_engine_base_t::mechanism_ready () {}
int stream_engine_base_t::pull_msg_from_session (msg_t *m) { return _session->pull_msg(m); }
int stream_engine_base_t::decode_and_push (msg_t *m) { return _session->push_msg(m); }
int stream_engine_base_t::process_handshake_command (msg_t *) { return 0; }
int stream_engine_base_t::next_handshake_command (msg_t *) { return 0; }
std::string stream_engine_base_t::get_peer_address(const options_t&) { return ""; }

} // namespace slk
