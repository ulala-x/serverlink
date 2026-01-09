/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_SOCKET_BASE_HPP_INCLUDED
#define SL_SOCKET_BASE_HPP_INCLUDED

#include <string>
#include <map>

#include "own.hpp"
#include "array.hpp"
#include "options.hpp"
#include "../msg/blob.hpp"
#include "../io/i_mailbox.hpp"
#include "../io/i_poll_events.hpp"
#include "../io/poller.hpp"
#include "../pipe/pipe.hpp"
#include "../util/clock.hpp"
#include "endpoint.hpp"

namespace slk
{
class ctx_t;
class msg_t;
class pipe_t;

class socket_base_t : public own_t,
                      public array_item_t<0>,
                      public i_poll_events,
                      public i_pipe_events
{
    friend class ctx_t;
    friend class reaper_t;

  public:
    // Returns false if object is not a socket
    bool check_tag () const;

    // Returns whether the socket is thread-safe
    bool is_thread_safe () const;

    // Create a socket of a specified type
    static socket_base_t *create (int type_, class ctx_t *parent_,
                                   uint32_t tid_, int sid_);

    // Returns the mailbox associated with this socket
    i_mailbox *get_mailbox () const;

    // Interrupt blocking call if the socket is stuck in one
    // This function can be called from a different thread!
    void stop ();

    // Interface for communication with the API layer
    int setsockopt (int option_, const void *optval_, size_t optvallen_);
    int getsockopt (int option_, void *optval_, size_t *optvallen_);
    int bind (const char *endpoint_uri_);
    int connect (const char *endpoint_uri_);
    int term_endpoint (const char *endpoint_uri_);
    int send (msg_t *msg_, int flags_);
    int recv (msg_t *msg_, int flags_);
    int close ();

    // These functions are used by the polling mechanism to determine
    // which events are to be reported from this socket
    bool has_in ();
    bool has_out ();

    // Using this function reaper thread ask the socket to register with
    // its poller
    void start_reaping (poller_t *poller_);

    // i_poll_events implementation. This interface is used when socket
    // is handled by the poller in the reaper thread
    void in_event () final;
    void out_event () final;
    void timer_event (int id_) final;

    // i_pipe_events interface implementation
    void read_activated (pipe_t *pipe_) final;
    void write_activated (pipe_t *pipe_) final;
    void hiccuped (pipe_t *pipe_) final;
    void pipe_terminated (pipe_t *pipe_) final;

    // Query the state of a specific peer. The default implementation
    // always returns an ENOTSUP error
    virtual int get_peer_state (const void *routing_id_,
                                size_t routing_id_size_) const;

    bool is_disconnected () const;

    // Signaler support for thread-safe sockets (not yet implemented)
    // These are stub methods that will be implemented when thread-safe sockets are added
    void add_signaler (class signaler_t *signaler_) { (void)signaler_; }
    void remove_signaler (class signaler_t *signaler_) { (void)signaler_; }

  protected:
    socket_base_t (class ctx_t *parent_, uint32_t tid_, int sid_,
                   bool thread_safe_ = false);
    virtual ~socket_base_t () override;

    // Concrete algorithms for the x- methods are to be defined by
    // individual socket types
    virtual void xattach_pipe (pipe_t *pipe_, bool subscribe_to_all_ = false,
                               bool locally_initiated_ = false) = 0;

    // The default implementation assumes there are no specific socket
    // options for the particular socket type. If not so, override this method
    virtual int xsetsockopt (int option_, const void *optval_,
                            size_t optvallen_);

    // The default implementation assumes there are no specific socket
    // options for the particular socket type. If not so, override this method
    virtual int xgetsockopt (int option_, void *optval_, size_t *optvallen_);

    // The default implementation assumes that send is not supported
    virtual bool xhas_out ();
    virtual int xsend (msg_t *msg_);

    // The default implementation assumes that recv in not supported
    virtual bool xhas_in ();
    virtual int xrecv (msg_t *msg_);

    // i_pipe_events will be forwarded to these functions
    virtual void xread_activated (pipe_t *pipe_);
    virtual void xwrite_activated (pipe_t *pipe_);
    virtual void xhiccuped (pipe_t *pipe_);
    virtual void xpipe_terminated (pipe_t *pipe_) = 0;

    // Delay actual destruction of the socket
    void process_destroy () final;

    int connect_internal (const char *endpoint_uri_);

    // Helper to access pipes for derived classes
    typedef array_t<pipe_t, 3> pipes_t;
    pipes_t &get_pipes () { return _pipes; }

    // Socket options
    options_t options;

  public:
    // Processes commands sent to this socket (if any). If timeout is -1,
    // returns only after at least one command was processed.
    // If throttle argument is true, commands are processed at most once
    // in a predefined time period.
    // This is public so that slk_poll() can process pending commands before polling.
    int process_commands (int timeout_, bool throttle_);

  private:
    // Creates new endpoint ID and adds the endpoint to the map
    void add_endpoint (const endpoint_uri_pair_t &endpoint_pair_,
                       own_t *endpoint_, pipe_t *pipe_);

    // Map of open endpoints
    typedef std::pair<own_t *, pipe_t *> endpoint_pipe_t;
    typedef std::multimap<std::string, endpoint_pipe_t> endpoints_t;
    endpoints_t _endpoints;

    // To be called after processing commands or invoking any command
    // handlers explicitly. If required, it will deallocate the socket
    void check_destroy ();

    // Moves the flags from the message to local variables,
    // to be later retrieved by getsockopt
    void extract_flags (const msg_t *msg_);

    // Used to check whether the object is a socket
    uint32_t _tag;

    // If true, associated context was already terminated
    bool _ctx_terminated;

    // If true, object should have been already destroyed. However,
    // destruction is delayed while we unwind the stack to the point
    // where it doesn't intersect the object being destroyed
    bool _destroyed;

    // Parse URI string
    static int parse_uri (const char *uri_, std::string &protocol_,
                         std::string &path_);

    // Check whether transport protocol, as specified in connect or
    // bind, is available and compatible with the socket type
    int check_protocol (const std::string &protocol_) const;

    // Register the pipe with this socket
    void attach_pipe (pipe_t *pipe_, bool subscribe_to_all_ = false,
                      bool locally_initiated_ = false);

    // Handlers for incoming commands
    void process_stop () final;
    void process_bind (pipe_t *pipe_) final;
    void process_term (int linger_) final;
    void process_term_endpoint (std::string *endpoint_) final;

    void update_pipe_options (int option_);

    std::string resolve_tcp_addr (std::string endpoint_uri_,
                                  const char *tcp_address_);

    // Socket's mailbox object
    i_mailbox *_mailbox;

    // List of attached pipes
    pipes_t _pipes;

    // Reaper's poller and handle of this socket within it
    poller_t *_poller;
    poller_t::handle_t _handle;

    // Timestamp of when commands were processed the last time
    uint64_t _last_tsc;

    // Number of messages received since last command processing
    int _ticks;

    // True if the last message received had MORE flag set
    bool _rcvmore;

    // Improves efficiency of time measurement
    clock_t _clock;

    // Last socket endpoint resolved URI
    std::string _last_endpoint;

    // Indicate if the socket is thread safe
    const bool _thread_safe;

    // Add a flag for mark disconnect action
    bool _disconnected;

    SL_NON_COPYABLE_NOR_MOVABLE (socket_base_t)
};

// Base class for sockets with routing capabilities (like ROUTER)
class routing_socket_base_t : public socket_base_t
{
  protected:
    routing_socket_base_t (class ctx_t *parent_, uint32_t tid_, int sid_);
    ~routing_socket_base_t () override;

    // methods from socket_base_t
    int xsetsockopt (int option_, const void *optval_,
                     size_t optvallen_) override;
    void xwrite_activated (pipe_t *pipe_) final;

    // own methods
    std::string extract_connect_routing_id ();
    bool connect_routing_id_is_set () const;

    struct out_pipe_t
    {
        pipe_t *pipe;
        bool active;
    };

    void add_out_pipe (blob_t routing_id_, pipe_t *pipe_);
    bool has_out_pipe (const blob_t &routing_id_) const;
    out_pipe_t *lookup_out_pipe (const blob_t &routing_id_);
    const out_pipe_t *lookup_out_pipe (const blob_t &routing_id_) const;
    void erase_out_pipe (const pipe_t *pipe_);
    out_pipe_t try_erase_out_pipe (const blob_t &routing_id_);

    template <typename Func>
    bool any_of_out_pipes (Func func_)
    {
        bool res = false;
        for (out_pipes_t::iterator it = _out_pipes.begin (),
                                   end = _out_pipes.end ();
             it != end && !res; ++it) {
            res |= func_ (*it->second.pipe);
        }
        return res;
    }

  private:
    // Outbound pipes indexed by the peer IDs
    typedef std::map<blob_t, out_pipe_t> out_pipes_t;
    out_pipes_t _out_pipes;

    // Next assigned name on a connect() call used by ROUTER socket
    std::string _connect_routing_id;
};
}

#endif
