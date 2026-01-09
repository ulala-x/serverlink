/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_STREAM_ENGINE_BASE_HPP_INCLUDED
#define SL_STREAM_ENGINE_BASE_HPP_INCLUDED

#include <string>
#include <vector>
#include <memory>

#include "../core/i_engine.hpp"
#include "../io/i_async_stream.hpp"
#include "../util/config.hpp"
#include "../core/options.hpp"
#include "../msg/msg.hpp"
#include "../protocol/i_encoder.hpp"
#include "../protocol/i_decoder.hpp"
#include <asio.hpp>

namespace slk
{
class io_thread_t;
class session_base_t;
class mechanism_t;
struct metadata_t;

class stream_engine_base_t : public i_engine
{
  public:
    stream_engine_base_t (std::unique_ptr<i_async_stream> stream_,
                           const options_t &options_,
                           const endpoint_uri_pair_t &endpoint_uri_pair_,
                           bool has_handshake_stage_);
    virtual ~stream_engine_base_t ();

    void plug (slk::io_thread_t *io_thread_, slk::session_base_t *session_) override;
    void terminate () override;
    bool restart_input () override;
    void restart_output () override;
    void zap_msg_available () override;
    const endpoint_uri_pair_t &get_endpoint () const override;

    void handle_read (size_t bt, int ec);
    void handle_write (size_t bt, int ec);

  protected:
    void error (error_reason_t reason_);
    virtual void process_handshake_data (unsigned char *data_, size_t size_) = 0;
    virtual int process_handshake_command (msg_t *msg_) = 0;
    virtual int next_handshake_command (msg_t *msg_) = 0;

    const options_t _options;
    bool _plugged, _handshaking, _has_handshake_timer;

    unsigned char *_inpos;
    size_t _insize;
    i_decoder *_decoder;

    unsigned char *_outpos;
    size_t _outsize;
    i_encoder *_encoder;

    // libzmq Parity Batching
    std::vector<asio::const_buffer> _out_batch;
    bool _is_vectorized;
    void fill_out_batch();

    mechanism_t *_mechanism;
    int (stream_engine_base_t::*_next_msg) (msg_t *msg_);
    int (stream_engine_base_t::*_process_msg) (msg_t *msg_);

    metadata_t *_metadata;
    bool _input_stopped, _output_stopped;
    const endpoint_uri_pair_t _endpoint_uri_pair;
    const std::string _peer_address;
    std::shared_ptr<int> _lifetime_sentinel;
    unsigned char _handshake_buffer[64];

  private:
    void unplug (), start_read (), start_write ();
    int pull_msg_from_session (msg_t *msg_), decode_and_push (msg_t *msg_);

  protected:
    void mechanism_ready ();

  private:
    std::unique_ptr<i_async_stream> _stream;
    msg_t _tx_msg;
    bool _io_error;
    slk::session_base_t *_session;
    bool _has_handshake_stage;

    SL_NON_COPYABLE_NOR_MOVABLE (stream_engine_base_t)
};
}

#endif