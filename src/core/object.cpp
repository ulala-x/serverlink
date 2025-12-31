/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "object.hpp"
#include "own.hpp"
#include "ctx.hpp"
#include "socket_base.hpp"
#include "../util/err.hpp"
#include "../pipe/pipe.hpp"
#include "../io/io_thread.hpp"
#include <string.h>

// Forward declarations for types we'll implement later
namespace slk
{
class session_base_t : public own_t
{
  public:
    session_base_t (io_thread_t *io_thread_, const options_t &options_)
        : own_t (io_thread_, options_) {}
    virtual ~session_base_t () {}
};
}

slk::object_t::object_t (ctx_t *ctx_, uint32_t tid_) : _ctx (ctx_), _tid (tid_)
{
}

slk::object_t::object_t (object_t *parent_) :
    _ctx (parent_->_ctx), _tid (parent_->_tid)
{
}

slk::object_t::~object_t ()
{
}

uint32_t slk::object_t::get_tid () const
{
    return _tid;
}

void slk::object_t::set_tid (uint32_t id_)
{
    _tid = id_;
}

slk::ctx_t *slk::object_t::get_ctx () const
{
    return _ctx;
}

void slk::object_t::process_command (const command_t &cmd_)
{
    switch (cmd_.type) {
        case command_t::activate_read:
            process_activate_read ();
            break;

        case command_t::activate_write:
            process_activate_write (cmd_.args.activate_write.msgs_read);
            break;

        case command_t::stop:
            process_stop ();
            break;

        case command_t::plug:
            process_plug ();
            process_seqnum ();
            break;

        case command_t::own:
            process_own (cmd_.args.own.object);
            process_seqnum ();
            break;

        case command_t::attach:
            process_attach (cmd_.args.attach.engine);
            process_seqnum ();
            break;

        case command_t::bind:
            process_bind (cmd_.args.bind.pipe);
            process_seqnum ();
            break;

        case command_t::hiccup:
            process_hiccup (cmd_.args.hiccup.pipe);
            break;

        case command_t::pipe_peer_stats:
            process_pipe_peer_stats (cmd_.args.pipe_peer_stats.queue_count,
                                     cmd_.args.pipe_peer_stats.socket_base,
                                     cmd_.args.pipe_peer_stats.endpoint_pair);
            break;

        case command_t::pipe_stats_publish:
            process_pipe_stats_publish (
              cmd_.args.pipe_stats_publish.outbound_queue_count,
              cmd_.args.pipe_stats_publish.inbound_queue_count,
              cmd_.args.pipe_stats_publish.endpoint_pair);
            break;

        case command_t::pipe_term:
            process_pipe_term ();
            break;

        case command_t::pipe_term_ack:
            process_pipe_term_ack ();
            break;

        case command_t::pipe_hwm:
            process_pipe_hwm (cmd_.args.pipe_hwm.inhwm,
                              cmd_.args.pipe_hwm.outhwm);
            break;

        case command_t::term_req:
            process_term_req (cmd_.args.term_req.object);
            break;

        case command_t::term:
            process_term (cmd_.args.term.linger);
            break;

        case command_t::term_ack:
            process_term_ack ();
            break;

        case command_t::term_endpoint:
            process_term_endpoint (cmd_.args.term_endpoint.endpoint);
            break;

        case command_t::reap:
            process_reap (cmd_.args.reap.socket);
            break;

        case command_t::reaped:
            process_reaped ();
            break;

        case command_t::inproc_connected:
            process_seqnum ();
            break;

        case command_t::conn_failed:
            process_conn_failed ();
            break;

        case command_t::done:
        default:
            slk_assert (false);
    }
}

int slk::object_t::register_endpoint (const char *addr_,
                                      const endpoint_t &endpoint_)
{
    return _ctx->register_endpoint (addr_, endpoint_);
}

int slk::object_t::unregister_endpoint (const std::string &addr_,
                                        socket_base_t *socket_)
{
    return _ctx->unregister_endpoint (addr_, socket_);
}

void slk::object_t::unregister_endpoints (socket_base_t *socket_)
{
    return _ctx->unregister_endpoints (socket_);
}

slk::endpoint_t slk::object_t::find_endpoint (const char *addr_) const
{
    return _ctx->find_endpoint (addr_);
}

void slk::object_t::pend_connection (const std::string &addr_,
                                     const endpoint_t &endpoint_,
                                     pipe_t **pipes_)
{
    _ctx->pend_connection (addr_, endpoint_, pipes_);
}

void slk::object_t::connect_pending (const char *addr_,
                                     slk::socket_base_t *bind_socket_)
{
    return _ctx->connect_pending (addr_, bind_socket_);
}

void slk::object_t::destroy_socket (socket_base_t *socket_)
{
    _ctx->destroy_socket (socket_);
}

slk::io_thread_t *slk::object_t::choose_io_thread (uint64_t affinity_) const
{
    return _ctx->choose_io_thread (affinity_);
}

void slk::object_t::send_stop ()
{
    // 'stop' command goes always from administrative thread to
    // the current object
    command_t cmd;
    cmd.destination = this;
    cmd.type = command_t::stop;
    _ctx->send_command (_tid, cmd);
}

void slk::object_t::send_plug (own_t *destination_, bool inc_seqnum_)
{
    if (inc_seqnum_)
        destination_->inc_seqnum ();

    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::plug;
    send_command (cmd);
}

void slk::object_t::send_own (own_t *destination_, own_t *object_)
{
    destination_->inc_seqnum ();
    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::own;
    cmd.args.own.object = object_;
    send_command (cmd);
}

void slk::object_t::send_attach (session_base_t *destination_,
                                 i_engine *engine_,
                                 bool inc_seqnum_)
{
    if (inc_seqnum_)
        destination_->inc_seqnum ();

    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::attach;
    cmd.args.attach.engine = engine_;
    send_command (cmd);
}

void slk::object_t::send_conn_failed (session_base_t *destination_)
{
    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::conn_failed;
    send_command (cmd);
}

void slk::object_t::send_bind (own_t *destination_,
                               pipe_t *pipe_,
                               bool inc_seqnum_)
{
    if (inc_seqnum_)
        destination_->inc_seqnum ();

    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::bind;
    cmd.args.bind.pipe = pipe_;
    send_command (cmd);
}

void slk::object_t::send_activate_read (pipe_t *destination_)
{
    SL_DEBUG_LOG("DEBUG: send_activate_read from thread %u to pipe %p (thread %u)\n",
            get_tid(), (void*)destination_, destination_->get_tid());
    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::activate_read;
    send_command (cmd);
}

void slk::object_t::send_activate_write (pipe_t *destination_,
                                         uint64_t msgs_read_)
{
    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::activate_write;
    cmd.args.activate_write.msgs_read = msgs_read_;
    send_command (cmd);
}

void slk::object_t::send_hiccup (pipe_t *destination_, void *pipe_)
{
    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::hiccup;
    cmd.args.hiccup.pipe = pipe_;
    send_command (cmd);
}

void slk::object_t::send_pipe_peer_stats (pipe_t *destination_,
                                          uint64_t queue_count_,
                                          own_t *socket_base_,
                                          endpoint_uri_pair_t *endpoint_pair_)
{
    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::pipe_peer_stats;
    cmd.args.pipe_peer_stats.queue_count = queue_count_;
    cmd.args.pipe_peer_stats.socket_base = socket_base_;
    cmd.args.pipe_peer_stats.endpoint_pair = endpoint_pair_;
    send_command (cmd);
}

void slk::object_t::send_pipe_stats_publish (
  own_t *destination_,
  uint64_t outbound_queue_count_,
  uint64_t inbound_queue_count_,
  endpoint_uri_pair_t *endpoint_pair_)
{
    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::pipe_stats_publish;
    cmd.args.pipe_stats_publish.outbound_queue_count = outbound_queue_count_;
    cmd.args.pipe_stats_publish.inbound_queue_count = inbound_queue_count_;
    cmd.args.pipe_stats_publish.endpoint_pair = endpoint_pair_;
    send_command (cmd);
}

void slk::object_t::send_pipe_term (pipe_t *destination_)
{
    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::pipe_term;
    send_command (cmd);
}

void slk::object_t::send_pipe_term_ack (pipe_t *destination_)
{
    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::pipe_term_ack;
    send_command (cmd);
}

void slk::object_t::send_pipe_hwm (pipe_t *destination_,
                                   int inhwm_,
                                   int outhwm_)
{
    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::pipe_hwm;
    cmd.args.pipe_hwm.inhwm = inhwm_;
    cmd.args.pipe_hwm.outhwm = outhwm_;
    send_command (cmd);
}

void slk::object_t::send_term_req (own_t *destination_, own_t *object_)
{
    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::term_req;
    cmd.args.term_req.object = object_;
    send_command (cmd);
}

void slk::object_t::send_term (own_t *destination_, int linger_)
{
    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::term;
    cmd.args.term.linger = linger_;
    send_command (cmd);
}

void slk::object_t::send_term_ack (own_t *destination_)
{
    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::term_ack;
    send_command (cmd);
}

void slk::object_t::send_term_endpoint (own_t *destination_,
                                        std::string *endpoint_)
{
    command_t cmd;
    cmd.destination = destination_;
    cmd.type = command_t::term_endpoint;
    cmd.args.term_endpoint.endpoint = endpoint_;
    send_command (cmd);
}

void slk::object_t::send_reap (class socket_base_t *socket_)
{
    command_t cmd;
    cmd.destination = _ctx->get_reaper ();
    cmd.type = command_t::reap;
    cmd.args.reap.socket = socket_;
    send_command (cmd);
}

void slk::object_t::send_reaped ()
{
    command_t cmd;
    cmd.destination = _ctx->get_reaper ();
    cmd.type = command_t::reaped;
    send_command (cmd);
}

void slk::object_t::send_inproc_connected (slk::socket_base_t *socket_)
{
    command_t cmd;
    cmd.destination = socket_;
    cmd.type = command_t::inproc_connected;
    send_command (cmd);
}

void slk::object_t::send_done ()
{
    command_t cmd;
    cmd.destination = NULL;
    cmd.type = command_t::done;
    _ctx->send_command (ctx_t::term_tid, cmd);
}

void slk::object_t::process_stop ()
{
    slk_assert (false);
}

void slk::object_t::process_plug ()
{
    slk_assert (false);
}

void slk::object_t::process_own (own_t *)
{
    slk_assert (false);
}

void slk::object_t::process_attach (i_engine *)
{
    slk_assert (false);
}

void slk::object_t::process_bind (pipe_t *)
{
    slk_assert (false);
}

void slk::object_t::process_activate_read ()
{
    slk_assert (false);
}

void slk::object_t::process_activate_write (uint64_t)
{
    slk_assert (false);
}

void slk::object_t::process_hiccup (void *)
{
    slk_assert (false);
}

void slk::object_t::process_pipe_peer_stats (uint64_t,
                                             own_t *,
                                             endpoint_uri_pair_t *)
{
    slk_assert (false);
}

void slk::object_t::process_pipe_stats_publish (uint64_t,
                                                uint64_t,
                                                endpoint_uri_pair_t *)
{
    slk_assert (false);
}

void slk::object_t::process_pipe_term ()
{
    slk_assert (false);
}

void slk::object_t::process_pipe_term_ack ()
{
    slk_assert (false);
}

void slk::object_t::process_pipe_hwm (int, int)
{
    slk_assert (false);
}

void slk::object_t::process_term_req (own_t *)
{
    slk_assert (false);
}

void slk::object_t::process_term (int)
{
    slk_assert (false);
}

void slk::object_t::process_term_ack ()
{
    slk_assert (false);
}

void slk::object_t::process_term_endpoint (std::string *)
{
    slk_assert (false);
}

void slk::object_t::process_reap (class socket_base_t *)
{
    slk_assert (false);
}

void slk::object_t::process_reaped ()
{
    slk_assert (false);
}

void slk::object_t::process_seqnum ()
{
    slk_assert (false);
}

void slk::object_t::process_conn_failed ()
{
    slk_assert (false);
}

void slk::object_t::send_command (const command_t &cmd_)
{
    _ctx->send_command (cmd_.destination->get_tid (), cmd_);
}
