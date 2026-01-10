/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_OBJECT_HPP_INCLUDED
#define SL_OBJECT_HPP_INCLUDED

#include <stdint.h>
#include <string>
#include "../util/macros.hpp"
#include "endpoint.hpp"

namespace slk
{
class ctx_t;
struct command_t;
struct i_engine;
struct pending_connection_t;
class pipe_t;
class socket_base_t;
class session_base_t;
class io_thread_t;
class own_t;

class object_t
{
  public:
    object_t (slk::ctx_t *parent_, uint32_t tid_);
    object_t (slk::object_t *parent_);
    virtual ~object_t ();

    uint32_t get_tid () const;
    void set_tid (uint32_t id_);
    slk::ctx_t *get_ctx () const;

    void process_command (const slk::command_t &cmd_);
    void send_inproc_connected (slk::socket_base_t *socket_);
    void send_bind (slk::own_t *destination_,
                    slk::pipe_t *pipe_,
                    bool inc_seqnum_ = true);

  protected:
    int register_endpoint (const char *addr_, const slk::endpoint_t &endpoint_);
    int unregister_endpoint (const std::string &addr_, socket_base_t *socket_);
    void unregister_endpoints (slk::socket_base_t *socket_);
    slk::endpoint_t find_endpoint (const char *addr_) const;
    void pend_connection (const std::string &addr_,
                          const endpoint_t &endpoint_,
                          pipe_t **pipes_);
    void connect_pending (const char *addr_, slk::socket_base_t *bind_socket_);
    void destroy_socket (slk::socket_base_t *socket_);

    slk::io_thread_t *choose_io_thread (uint64_t affinity_) const;

    //  Command handlers.
    virtual void process_stop ();
    virtual void process_plug ();
    virtual void process_own (class own_t *object_);
    virtual void process_attach (class i_engine *engine_);
    virtual void process_bind (class pipe_t *pipe_);
    virtual void process_activate_read ();
    virtual void process_activate_write (uint64_t msgs_read_);
    virtual void process_hiccup (void *pipe_);
    virtual void process_pipe_term ();
    virtual void process_pipe_term_ack ();
    virtual void process_pipe_hwm (int inhwm_, int outhwm_);
    virtual void process_term_req (class own_t *object_);
    virtual void process_term (int linger_);
    virtual void process_term_ack ();
    virtual void process_term_endpoint (std::string *endpoint_);
    virtual void process_reAP (int interval_);
    virtual void process_reconnect (class i_engine *engine_);
    virtual void process_pipe_peer_stats (uint64_t queue_count_,
                                          class own_t *socket_base_,
                                          struct endpoint_uri_pair_t *endpoint_pair_);
    virtual void process_pipe_stats_publish (uint64_t outbound_queue_count_,
                                             uint64_t inbound_queue_count_,
                                             struct endpoint_uri_pair_t *endpoint_pair_);
    virtual void process_seqnum ();
    virtual void process_reap (class socket_base_t *socket_);
    virtual void process_reaped ();
    virtual void process_conn_failed ();

    //  Command senders.
    void send_stop ();
    void send_plug (slk::own_t *destination_, bool inc_seqnum_ = true);
    void send_own (slk::own_t *destination_, slk::own_t *object_);
    void send_attach (class session_base_t *destination_,
                      slk::i_engine *engine_,
                      bool inc_seqnum_ = true);
    void send_activate_read (slk::pipe_t *destination_);
    void send_activate_write (slk::pipe_t *destination_, uint64_t msgs_read_);
    void send_hiccup (slk::pipe_t *destination_, void *pipe_);
    void send_pipe_term (slk::pipe_t *destination_);
    void send_pipe_term_ack (slk::pipe_t *destination_);
    void send_pipe_hwm (slk::pipe_t *destination_, int inhwm_, int outhwm_);
    void send_term_req (class own_t *destination_, class own_t *object_);
    void send_term (slk::own_t *destination_, int linger_);
    void send_term_ack (slk::own_t *destination_);
    void send_term_endpoint (class own_t *destination_, std::string *endpoint_);
    void send_reAP (class monitor_t *destination_, int interval_);
    void send_reconnect (class session_base_t *destination_,
                         class i_engine *engine_);
    void send_pipe_peer_stats (slk::pipe_t *destination_,
                               uint64_t queue_count_,
                               class own_t *socket_base_,
                               struct endpoint_uri_pair_t *endpoint_pair_);
    void send_pipe_stats_publish (class own_t *destination_,
                                  uint64_t outbound_queue_count_,
                                  uint64_t inbound_queue_count_,
                                  struct endpoint_uri_pair_t *endpoint_pair_);
    void send_reap (class socket_base_t *socket_);
    void send_reaped ();
    void send_done ();
    void send_conn_failed (class session_base_t *destination_);

  private:
    slk::ctx_t *_ctx;
    uint32_t _tid;

    SL_NON_COPYABLE_NOR_MOVABLE (object_t)
};
}

#endif