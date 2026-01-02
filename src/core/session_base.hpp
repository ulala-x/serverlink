/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_SESSION_BASE_HPP_INCLUDED
#define SL_SESSION_BASE_HPP_INCLUDED

#include <set>

#include "own.hpp"
#include "../io/io_object.hpp"
#include "../pipe/pipe.hpp"
#include "socket_base.hpp"
#include "i_engine.hpp"
#include "../msg/msg.hpp"

namespace slk
{
class io_thread_t;
struct i_engine;
struct address_t;

class session_base_t : public own_t, public io_object_t, public i_pipe_events
{
  public:
    //  Create a session
    static session_base_t *create (slk::io_thread_t *io_thread_,
                                   bool active_,
                                   slk::socket_base_t *socket_,
                                   const options_t &options_,
                                   address_t *addr_);

    //  To be used once only, when creating the session.
    void attach_pipe (slk::pipe_t *pipe_);

    //  Following functions are the interface exposed towards the engine.
    virtual void reset ();
    void flush ();
    void rollback ();
    void engine_error (bool handshaked_, slk::i_engine::error_reason_t reason_);
    void engine_ready ();

    //  i_pipe_events interface implementation.
    void read_activated (slk::pipe_t *pipe_) final;
    void write_activated (slk::pipe_t *pipe_) final;
    void hiccuped (slk::pipe_t *pipe_) final;
    void pipe_terminated (slk::pipe_t *pipe_) final;

    //  Delivers a message. Returns 0 if successful; -1 otherwise.
    //  The function takes ownership of the message.
    virtual int push_msg (msg_t *msg_);

    //  Fetches a message. Returns 0 if successful; -1 otherwise.
    //  The caller is responsible for freeing the message when no
    //  longer used.
    virtual int pull_msg (msg_t *msg_);

    socket_base_t *get_socket () const;
    const endpoint_uri_pair_t &get_endpoint () const;

  protected:
    session_base_t (slk::io_thread_t *io_thread_,
                    bool active_,
                    slk::socket_base_t *socket_,
                    const options_t &options_,
                    address_t *addr_);
    ~session_base_t () override;

  private:
    void start_connecting (bool wait_);

    void reconnect ();

    //  Handlers for incoming commands.
    void process_plug () final;
    void process_attach (slk::i_engine *engine_) final;
    void process_term (int linger_) final;
    void process_conn_failed () final;

    //  i_poll_events handlers.
    void timer_event (int id_) final;

    //  Remove any half processed messages. Flush unflushed messages.
    //  Call this function when engine disconnect to get rid of leftovers.
    void clean_pipes ();

    //  If true, this session (re)connects to the peer. Otherwise, it's
    //  a transient session created by the listener.
    const bool _active;

    //  Pipe connecting the session to its socket.
    slk::pipe_t *_pipe;

    //  This set is added to with pipes we are disconnecting, but haven't yet completed
    std::set<pipe_t *> _terminating_pipes;

    //  This flag is true if the remainder of the message being processed
    //  is still in the in pipe.
    bool _incomplete_in;

    //  True if termination have been suspended to push the pending
    //  messages to the network.
    bool _pending;

    //  The protocol I/O engine connected to the session.
    slk::i_engine *_engine;

    //  The socket the session belongs to.
    slk::socket_base_t *_socket;

    //  I/O thread the session is living in. It will be used to plug in
    //  the engines into the same thread.
    slk::io_thread_t *_io_thread;

    //  ID of the linger timer
    enum
    {
        linger_timer_id = 0x20
    };

    //  True is linger timer is running.
    bool _has_linger_timer;

    //  Protocol and address to use when connecting.
    address_t *_addr;

    SL_NON_COPYABLE_NOR_MOVABLE (session_base_t)
};
}

#endif
