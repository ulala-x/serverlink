/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_OBJECT_HPP_INCLUDED
#define SL_OBJECT_HPP_INCLUDED

#include <stdint.h>
#include <string>
#include "../util/macros.hpp"

namespace slk
{
class ctx_t;
class pipe_t;
class own_t;
struct command_t;
struct endpoint_uri_pair_t;

class object_t
{
  public:
    object_t (slk::ctx_t *parent_, uint32_t tid_);
    object_t (slk::object_t *parent_);
    virtual ~object_t () = default;

    uint32_t get_tid () const;
    ctx_t *get_ctx ();

    void process_command (const slk::command_t &cmd_);

    void send_hiccup (slk::pipe_t *destination_);
    void send_term (slk::own_t *destination_, int linger_);
    void send_activate_read (slk::pipe_t *destination_);
    void send_activate_write (slk::pipe_t *destination_, uint64_t msgs_read_);

  protected:
    virtual void process_stop ();
    virtual void process_plug ();
    virtual void process_own (slk::own_t *object_);
    virtual void process_attach (class i_engine *engine_);
    virtual void process_bind (slk::pipe_t *pipe_);
    virtual void process_activate_read ();
    virtual void process_activate_write (uint64_t msgs_read_);
    virtual void process_hiccup (void *pipe_);
    virtual void process_pipe_term ();
    virtual void process_pipe_term_ack ();
    virtual void process_term (int linger_);
    virtual void process_term_ack ();
    virtual void process_term_endpoint (std::string *endpoint_);
    virtual void process_conn_failed ();
    virtual void process_pipe_peer_stats (uint64_t, slk::own_t*, endpoint_uri_pair_t*);
    virtual void process_pipe_hwm (int, int);

  private:
    void send_command (const slk::command_t &cmd_);
    slk::ctx_t *_parent;
    uint32_t _tid;

    SL_NON_COPYABLE_NOR_MOVABLE (object_t)
};
}

#endif
