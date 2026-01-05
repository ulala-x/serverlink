/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_STREAM_ENGINE_BASE_HPP_INCLUDED
#define SL_STREAM_ENGINE_BASE_HPP_INCLUDED

#include <stddef.h>
#include <memory>

#include "../core/i_engine.hpp"
#include "i_encoder.hpp"
#include "i_decoder.hpp"
#include "../core/options.hpp"
#include "../core/socket_base.hpp"
#include "../msg/metadata.hpp"
#include "../msg/msg.hpp"
#include "../transport/tcp.hpp"
#include "../io/i_async_stream.hpp" // New stream interface

namespace slk
{
class io_thread_t;
class session_base_t;
class mechanism_t;

//  This engine handles any socket with SOCK_STREAM semantics,
//  e.g. TCP socket or an UNIX domain socket.

class stream_engine_base_t : public i_engine
{
  public:
    stream_engine_base_t (std::unique_ptr<i_async_stream> stream,
                          const options_t &options_,
                          const endpoint_uri_pair_t &endpoint_uri_pair_,
                          bool has_handshake_stage_);
    ~stream_engine_base_t () override;

    //  i_engine interface implementation.
    bool has_handshake_stage () final { return _has_handshake_stage; };
    void plug (slk::io_thread_t *io_thread_,
               slk::session_base_t *session_) final;
    void terminate () final;
    bool restart_input () final;
    void restart_output () final;
    void zap_msg_available () final;
    const endpoint_uri_pair_t &get_endpoint () const final;

  protected:
    // Asynchronous operation handlers
    void start_read();
    void handle_read(size_t bytes_transferred, int error);
    void start_write();
    void handle_write(size_t bytes_transferred, int error);

    // Called from handle_read during handshake phase.
    // Must be implemented by derived classes to process greeting data.
    // Should call set_handshake_complete() when done.
    virtual void process_handshake_data(unsigned char* buffer, size_t size) = 0;

    //  Allows derived classes to signal that the handshake is finished.
    void set_handshake_complete() { _handshaking = false; }


    typedef metadata_t::dict_t properties_t;
    bool init_properties (properties_t &properties_);

    //  Function to handle network disconnections.
    virtual void error (error_reason_t reason_);

    int next_handshake_command (msg_t *msg_);
    int process_handshake_command (msg_t *msg_);

    int pull_msg_from_session (msg_t *msg_);
    int push_msg_to_session (msg_t *msg_);

    int pull_and_encode (msg_t *msg_);
    virtual int decode_and_push (msg_t *msg_);
    int push_one_then_decode_and_push (msg_t *msg_);

    void set_handshake_timer ();

    virtual void plug_internal (){};

    virtual int process_command_message (msg_t *msg_)
    {
        SL_UNUSED (msg_);
        return -1;
    };
    virtual int produce_ping_message (msg_t *msg_)
    {
        SL_UNUSED (msg_);
        return -1;
    };
    virtual int process_heartbeat_message (msg_t *msg_)
    {
        SL_UNUSED (msg_);
        return -1;
    };
    virtual int produce_pong_message (msg_t *msg_)
    {
        SL_UNUSED (msg_);
        return -1;
    };

    session_base_t *session () { return _session; }
    socket_base_t *socket () { return _socket; }

    const options_t _options;

    // When true, we are still trying to determine whether
    // the peer is using versioned protocol, and if so, which
    // version. When false, normal message flow has started.
    bool _handshaking;

    unsigned char *_inpos;
    size_t _insize;
    i_decoder *_decoder;

    unsigned char *_outpos;
    size_t _outsize;
    i_encoder *_encoder;

    mechanism_t *_mechanism;

    int (stream_engine_base_t::*_next_msg) (msg_t *msg_);
    int (stream_engine_base_t::*_process_msg) (msg_t *msg_);

    //  Metadata to be attached to received messages. May be NULL.
    metadata_t *_metadata;

    //  True iff the engine couldn't consume the last decoded message.
    bool _input_stopped;

    //  True iff the engine doesn't have any message to encode.
    bool _output_stopped;

    //  Representation of the connected endpoints.
    const endpoint_uri_pair_t _endpoint_uri_pair;

    //  ID of the handshake timer
    enum
    {
        handshake_timer_id = 0x40
    };

    //  True is linger timer is running.
    bool _has_handshake_timer;

    //  Heartbeat stuff
    enum
    {
        heartbeat_ivl_timer_id = 0x80,
        heartbeat_timeout_timer_id = 0x81,
        heartbeat_ttl_timer_id = 0x82
    };
    bool _has_ttl_timer;
    bool _has_timeout_timer;
    bool _has_heartbeat_timer;


    const std::string _peer_address;

  private:
    //  Unplug the engine from the session.
    void unplug ();

    int write_credential (msg_t *msg_);

  protected:
    void mechanism_ready ();

  private:
    //  The underlying asynchronous stream.
    std::unique_ptr<i_async_stream> _stream;

    bool _plugged;

    msg_t _tx_msg;

    bool _io_error;

    //  The session this engine is attached to.
    slk::session_base_t *_session;

    //  Socket
    slk::socket_base_t *_socket;

    //  Indicate if engine has an handshake stage, if it does, engine must call session.engine_ready
    //  when handshake is completed.
    bool _has_handshake_stage;

    SL_NON_COPYABLE_NOR_MOVABLE (stream_engine_base_t)
};
}

#endif
