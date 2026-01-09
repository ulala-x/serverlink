/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "precompiled.hpp"
#include <new>
#include <string>
#include <sstream>
#include <algorithm>

#include "../util/macros.hpp"
#include "socket_base.hpp"
#include "../transport/tcp_listener.hpp"
#include "../transport/tcp_connecter.hpp"
#if defined SL_HAVE_IPC
#include "../transport/ipc_listener.hpp"
#endif
#include "../io/io_thread.hpp"
#include "session_base.hpp"
#include "../util/config.hpp"
#include "../pipe/pipe.hpp"
#include "../util/err.hpp"
#include "ctx.hpp"
#include "../util/likely.hpp"
#include "../msg/msg.hpp"
#include "../transport/address.hpp"
#include "../transport/tcp_address.hpp"
#include "../io/mailbox.hpp"

#include "pair.hpp"
#include "router.hpp"
#include "pub.hpp"
#include "sub.hpp"
#include "xsub.hpp"
#include "xpub.hpp"

namespace slk
{
// Note: inbound_poll_rate and max_command_delay are defined in config.hpp
}

bool slk::socket_base_t::check_tag () const
{
    return _tag == 0xbaddecaf;
}

bool slk::socket_base_t::is_thread_safe () const
{
    return _thread_safe;
}

slk::socket_base_t *slk::socket_base_t::create (int type_,
                                                 class ctx_t *parent_,
                                                 uint32_t tid_,
                                                 int sid_)
{
    socket_base_t *s = NULL;
    switch (type_) {
        case SL_PAIR:
            s = new (std::nothrow) pair_t (parent_, tid_, sid_);
            break;
        case SL_ROUTER:
            s = new (std::nothrow) router_t (parent_, tid_, sid_);
            break;
        case SL_PUB:
            s = new (std::nothrow) pub_t (parent_, tid_, sid_);
            break;
        case SL_SUB:
            s = new (std::nothrow) sub_t (parent_, tid_, sid_);
            break;
        case SL_XSUB:
            s = new (std::nothrow) xsub_t (parent_, tid_, sid_);
            break;
        case SL_XPUB:
            s = new (std::nothrow) xpub_t (parent_, tid_, sid_);
            break;
        default:
            errno = EINVAL;
            return NULL;
    }

    alloc_assert (s);

    if (s->_mailbox == NULL) {
        s->_destroyed = true;
        delete s;
        return NULL;
    }

    return s;
}

slk::socket_base_t::socket_base_t (ctx_t *parent_,
                                   uint32_t tid_,
                                   int sid_,
                                   bool thread_safe_) :
    own_t (parent_, tid_),
    _tag (0xbaddecaf),
    _ctx_terminated (false),
    _destroyed (false),
    _poller (NULL),
    _handle (static_cast<poller_t::handle_t> (NULL)),
    _last_tsc (0),
    _ticks (0),
    _rcvmore (false),
    _thread_safe (thread_safe_),
    _disconnected (false)
{
    options.socket_id = sid_;
    options.ipv6 = (parent_->get (SL_IPV6) != 0);
    options.linger.store (parent_->get (SL_BLOCKY) ? -1 : 0);
    options.zero_copy = parent_->get (SL_ZERO_COPY_RECV) != 0;

    // Only non-thread-safe sockets for now
    if (_thread_safe) {
        // Thread-safe sockets not implemented
        _mailbox = NULL;
    } else {
        mailbox_t *m = new (std::nothrow) mailbox_t ();
        slk_assert (m);

        if (m->get_fd () != retired_fd)
            _mailbox = m;
        else {
            delete m;
            _mailbox = NULL;
        }
    }
}

int slk::socket_base_t::get_peer_state (const void *routing_id_,
                                        size_t routing_id_size_) const
{
    SL_UNUSED (routing_id_);
    SL_UNUSED (routing_id_size_);

    // Only ROUTER sockets support this
    errno = ENOTSUP;
    return -1;
}

slk::socket_base_t::~socket_base_t ()
{
    if (_mailbox)
        delete _mailbox;

    slk_assert (_destroyed);
}

slk::i_mailbox *slk::socket_base_t::get_mailbox () const
{
    return _mailbox;
}

void slk::socket_base_t::stop ()
{
    // Called by ctx when it is terminated
    // 'stop' command is sent from the threads that called ctx term to
    // the thread owning the socket
    send_stop ();
}

int slk::socket_base_t::parse_uri (const char *uri_,
                                   std::string &protocol_,
                                   std::string &path_)
{
    slk_assert (uri_ != NULL);

    const std::string uri (uri_);
    const std::string::size_type pos = uri.find ("://");
    if (pos == std::string::npos) {
        errno = EINVAL;
        return -1;
    }
    protocol_ = uri.substr (0, pos);
    path_ = uri.substr (pos + 3);

    if (protocol_.empty () || path_.empty ()) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int slk::socket_base_t::check_protocol (const std::string &protocol_) const
{
    // ServerLink supports TCP protocol
    if (protocol_ == slk::protocol_name::tcp) {
        return 0;
    }

#if defined SL_HAVE_IPC
    // IPC protocol is supported on Unix-like systems
    if (protocol_ == slk::protocol_name::ipc) {
        return 0;
    }
#endif

    // Inproc protocol is always supported
    if (protocol_ == slk::protocol_name::inproc) {
        return 0;
    }

    // Unsupported protocol
    errno = EPROTONOSUPPORT;
    return -1;
}

void slk::socket_base_t::attach_pipe (pipe_t *pipe_,
                                      bool subscribe_to_all_,
                                      bool locally_initiated_)
{
    // First, register the pipe so that we can terminate it later on
    pipe_->set_event_sink (this);
    _pipes.push_back (pipe_);

    // Let the derived socket type know about new pipe
    xattach_pipe (pipe_, subscribe_to_all_, locally_initiated_);

    // If the socket is already being closed, ask any new pipes to terminate
    // straight away
    if (is_terminating ()) {
        register_term_acks (1);
        pipe_->terminate (false);
    }
}

int slk::socket_base_t::setsockopt (int option_,
                                    const void *optval_,
                                    size_t optvallen_)
{
    if (unlikely (_ctx_terminated)) {
        errno = ETERM;
        return -1;
    }

    // First, check whether specific socket type overloads the option
    int rc = xsetsockopt (option_, optval_, optvallen_);
    if (rc == 0 || errno != EINVAL) {
        return rc;
    }

    // If the socket type doesn't support the option, pass it to
    // the generic option parser
    rc = options.setsockopt (option_, optval_, optvallen_);
    update_pipe_options (option_);

    return rc;
}

int slk::socket_base_t::getsockopt (int option_,
                                    void *optval_,
                                    size_t *optvallen_)
{
    if (unlikely (_ctx_terminated)) {
        errno = ETERM;
        return -1;
    }

    // First, check whether specific socket type overloads the option
    int rc = xgetsockopt (option_, optval_, optvallen_);
    if (rc == 0 || errno != EINVAL) {
        return rc;
    }

    if (option_ == SL_RCVMORE) {
        return do_getsockopt<int> (optval_, optvallen_, _rcvmore ? 1 : 0);
    }

    if (option_ == SL_TYPE) {
        return do_getsockopt<int> (optval_, optvallen_, options.type);
    }

    if (option_ == SL_FD) {
        if (_mailbox == NULL) {
            errno = EINVAL;
            return -1;
        }
        // Cast to mailbox_t to access get_fd() method
        mailbox_t *mb = static_cast<mailbox_t *> (_mailbox);
        return do_getsockopt<fd_t> (optval_, optvallen_, mb->get_fd ());
    }

    if (option_ == SL_EVENTS) {
        // Process commands first to ensure we have up-to-date event state
        rc = process_commands (0, false);
        if (unlikely (rc != 0)) {
            return -1;
        }

        int events = 0;
        if (has_out ())
            events |= SL_POLLOUT;
        if (has_in ())
            events |= SL_POLLIN;

        return do_getsockopt<int> (optval_, optvallen_, events);
    }

    if (option_ == SL_LAST_ENDPOINT) {
        if (*optvallen_ < _last_endpoint.size () + 1) {
            errno = EINVAL;
            return -1;
        }
        memcpy (optval_, _last_endpoint.c_str (), _last_endpoint.size () + 1);
        *optvallen_ = _last_endpoint.size () + 1;
        return 0;
    }

    return options.getsockopt (option_, optval_, optvallen_);
}

int slk::socket_base_t::bind (const char *endpoint_uri_)
{
    if (unlikely (_ctx_terminated)) {
        errno = ETERM;
        return -1;
    }

    // Process pending commands, if any
    int rc = process_commands (0, false);
    if (unlikely (rc != 0)) {
        return -1;
    }

    // Parse endpoint_uri_ string
    std::string protocol;
    std::string address;
    if (parse_uri (endpoint_uri_, protocol, address)
        || check_protocol (protocol)) {
        return -1;
    }

    if (protocol == slk::protocol_name::tcp) {
        // Choose the I/O thread to run the listener in
        io_thread_t *io_thread = choose_io_thread (options.affinity);
        if (!io_thread) {
            errno = SL_EMTHREAD;
            return -1;
        }

        tcp_listener_t *listener =
          new (std::nothrow) tcp_listener_t (io_thread, this, options);
        alloc_assert (listener);
        rc = listener->set_local_address (address.c_str ());
        if (rc != 0) {
            delete listener;
            return -1;
        }

        // Save last endpoint URI
        listener->get_local_address (_last_endpoint);

        add_endpoint (make_unconnected_bind_endpoint_pair (_last_endpoint),
                      static_cast<own_t *> (listener), NULL);
        options.connected = true;
        return 0;
    }
#if defined SL_HAVE_IPC
    else if (protocol == slk::protocol_name::ipc) {
        // Choose the I/O thread to run the listener in
        io_thread_t *io_thread = choose_io_thread (options.affinity);
        if (!io_thread) {
            errno = SL_EMTHREAD;
            return -1;
        }

        ipc_listener_t *listener =
          new (std::nothrow) ipc_listener_t (io_thread, this, options);
        alloc_assert (listener);
        rc = listener->set_local_address (address.c_str ());
        if (rc != 0) {
            delete listener;
            return -1;
        }

        // Save last endpoint URI
        listener->get_local_address (_last_endpoint);

        add_endpoint (make_unconnected_bind_endpoint_pair (_last_endpoint),
                      static_cast<own_t *> (listener), NULL);
        options.connected = true;
        return 0;
    }
#endif
    else if (protocol == slk::protocol_name::inproc) {
        // inproc: register endpoint in context
        const endpoint_t endpoint (this, options);
        rc = get_ctx ()->register_endpoint (address.c_str (), endpoint);
        if (rc != 0) {
            return -1;
        }

        // Save last endpoint URI
        std::stringstream s;
        s << protocol << "://" << address;
        _last_endpoint = s.str ();

        // For inproc, we don't have a listener object, so pass NULL
        add_endpoint (make_unconnected_bind_endpoint_pair (_last_endpoint),
                      NULL, NULL);

        // Connect any pending connections for this endpoint
        get_ctx ()->connect_pending (address.c_str (), this);

        options.connected = true;
        return 0;
    }

    slk_assert (false);
    return -1;
}

int slk::socket_base_t::connect (const char *endpoint_uri_)
{
    return connect_internal (endpoint_uri_);
}

int slk::socket_base_t::connect_internal (const char *endpoint_uri_)
{
    if (unlikely (_ctx_terminated)) {
        errno = ETERM;
        return -1;
    }

    // Process pending commands, if any
    int rc = process_commands (0, false);
    if (unlikely (rc != 0)) {
        return -1;
    }

    // Parse endpoint_uri_ string
    std::string protocol;
    std::string address;
    if (parse_uri (endpoint_uri_, protocol, address)
        || check_protocol (protocol)) {
        return -1;
    }

    // Handle inproc protocol specially (no I/O thread needed)
    if (protocol == slk::protocol_name::inproc) {
        // Find the bound endpoint
        endpoint_t peer = get_ctx ()->find_endpoint (address.c_str ());
        if (!peer.socket) {
            // Endpoint not found, queue this connection as pending
            // Create a bi-directional pipe
            object_t *parents[2] = {this, NULL};
            pipe_t *new_pipes[2] = {NULL, NULL};

            int hwms[2] = {options.sndhwm, options.rcvhwm};
            bool conflates[2] = {false, false};
            rc = pipepair (parents, new_pipes, hwms, conflates);
            errno_assert (rc == 0);

            // Note: We can't set HWM boost yet since peer doesn't exist
            // It will be set when the bind happens and the connection is established

            // The peer doesn't exist yet so we don't know whether
            // to send the routing id message or not. To resolve this,
            // we always send our routing id and drop it later if
            // the peer doesn't expect it.
            send_routing_id (new_pipes[0], options);

            // Save last endpoint URI
            std::stringstream s;
            s << protocol << "://" << address;
            _last_endpoint = s.str ();

            // Add endpoint pair
            add_endpoint (make_unconnected_connect_endpoint_pair (_last_endpoint),
                          NULL, new_pipes[0]);

            // Queue the pending connection
            const endpoint_t local_endpoint (this, options);
            get_ctx ()->pend_connection (address, local_endpoint, new_pipes);

            return 0;
        }

        // Endpoint found, connect directly
        // Create a bi-directional pipe
        object_t *parents[2] = {this, peer.socket};
        pipe_t *new_pipes[2] = {NULL, NULL};

        int hwms[2] = {options.sndhwm, peer.options.rcvhwm};
        bool conflates[2] = {false, false};
        rc = pipepair (parents, new_pipes, hwms, conflates);
        errno_assert (rc == 0);

        // Set HWM boost for inproc
        new_pipes[0]->set_hwms_boost (peer.options.sndhwm, peer.options.rcvhwm);
        new_pipes[1]->set_hwms_boost (options.sndhwm, options.rcvhwm);

        // CRITICAL FIX: Send routing IDs BEFORE sending bind command
        // This ensures that when the peer processes the bind command and calls
        // identify_peer(), the routing ID is already available in the pipe.
        // Without this, identify_peer() fails and the pipe is not added to the
        // fair queue, making messages unreadable.

        // If required, send the routing id of the local socket to the peer
        if (peer.options.recv_routing_id) {
            msg_t routing_id_msg;
            int rc = routing_id_msg.init_size (options.routing_id_size);
            errno_assert (rc == 0);
            memcpy (routing_id_msg.data (), options.routing_id, options.routing_id_size);
            routing_id_msg.set_flags (msg_t::routing_id);
            const bool ok = new_pipes[0]->write (&routing_id_msg);
            slk_assert (ok);
            rc = routing_id_msg.close ();
            errno_assert (rc == 0);
        }

        // If required, send the routing id of the peer to the local socket
        if (options.recv_routing_id) {
            msg_t routing_id_msg;
            int rc = routing_id_msg.init_size (peer.options.routing_id_size);
            errno_assert (rc == 0);
            memcpy (routing_id_msg.data (), peer.options.routing_id, peer.options.routing_id_size);
            routing_id_msg.set_flags (msg_t::routing_id);
            const bool ok = new_pipes[1]->write (&routing_id_msg);
            slk_assert (ok);
            rc = routing_id_msg.close ();
            errno_assert (rc == 0);
        }

        // Flush both pipes once at the end
        if (peer.options.recv_routing_id)
            new_pipes[0]->flush ();
        if (options.recv_routing_id)
            new_pipes[1]->flush ();

        // Attach remote end of the pipe to the peer socket
        // We need to send a bind command to the peer
        // This must come AFTER sending routing IDs
        send_bind (peer.socket, new_pipes[1], false);

        // Attach local end of the pipe to this socket
        attach_pipe (new_pipes[0], false, true);

        // Save last endpoint URI
        std::stringstream s;
        s << protocol << "://" << address;
        _last_endpoint = s.str ();

        // Add endpoint
        add_endpoint (make_unconnected_connect_endpoint_pair (_last_endpoint),
                      NULL, new_pipes[0]);

        return 0;
    }

    // Choose the I/O thread to run the session in
    io_thread_t *io_thread = choose_io_thread (options.affinity);
    if (!io_thread) {
        errno = SL_EMTHREAD;
        return -1;
    }

    address_t *paddr =
      new (std::nothrow) address_t (protocol, address, this->get_ctx ());
    alloc_assert (paddr);

    // Resolve address (if needed by the protocol)
    if (protocol == slk::protocol_name::tcp) {
        // Basic sanity check on tcp:// address syntax
        const char *check = address.c_str ();
        if (isalnum (*check) || isxdigit (*check) || *check == '[') {
            check++;
            while (isalnum (*check) || isxdigit (*check) || *check == '.'
                   || *check == '-' || *check == ':' || *check == '%'
                   || *check == '[' || *check == ']') {
                check++;
            }
        }
        // Assume the worst, now look for success
        rc = -1;
        // Did we reach the end of the address safely?
        if (*check == 0) {
            // Do we have a valid port string?
            check = strrchr (address.c_str (), ':');
            if (check) {
                check++;
                if (*check && (isdigit (*check)))
                    rc = 0; // Valid
            }
        }
        if (rc == -1) {
            errno = EINVAL;
            delete paddr;
            return -1;
        }
        // Defer resolution until a socket is opened
        paddr->resolved.tcp_addr = NULL;
    }

    // Create session
    session_base_t *session =
      session_base_t::create (io_thread, true, this, options, paddr);
    errno_assert (session);

    const bool subscribe_to_all = false;
    pipe_t *newpipe = NULL;

    if (options.immediate != 1 || subscribe_to_all) {
        // Create a bi-directional pipe
        object_t *parents[2] = {this, session};
        pipe_t *new_pipes[2] = {NULL, NULL};

        int hwms[2] = {options.sndhwm, options.rcvhwm};
        bool conflates[2] = {false, false};
        rc = pipepair (parents, new_pipes, hwms, conflates);
        errno_assert (rc == 0);

        // Attach local end of the pipe to the socket object
        attach_pipe (new_pipes[0], subscribe_to_all, true);
        newpipe = new_pipes[0];

        // Attach remote end of the pipe to the session object later on
        session->attach_pipe (new_pipes[1]);
    }

    // Save last endpoint URI
    paddr->to_string (_last_endpoint);

    add_endpoint (make_unconnected_connect_endpoint_pair (endpoint_uri_),
                  static_cast<own_t *> (session), newpipe);
    return 0;
}

std::string slk::socket_base_t::resolve_tcp_addr (std::string endpoint_uri_,
                                                   const char *tcp_address_)
{
    // The resolved last_endpoint is used as a key in the endpoints map
    if (_endpoints.find (endpoint_uri_) == _endpoints.end ()) {
        tcp_address_t *tcp_addr = new (std::nothrow) tcp_address_t ();
        alloc_assert (tcp_addr);
        int rc = tcp_addr->resolve (tcp_address_, false, options.ipv6);

        if (rc == 0) {
            tcp_addr->to_string (endpoint_uri_);
            if (_endpoints.find (endpoint_uri_) == _endpoints.end ()) {
                rc = tcp_addr->resolve (tcp_address_, true, options.ipv6);
                if (rc == 0) {
                    tcp_addr->to_string (endpoint_uri_);
                }
            }
        }
        delete tcp_addr;
    }
    return endpoint_uri_;
}

void slk::socket_base_t::add_endpoint (const endpoint_uri_pair_t &endpoint_pair_,
                                        own_t *endpoint_,
                                        pipe_t *pipe_)
{
    // Activate the session. Make it a child of this socket
    // For inproc connections, endpoint_ may be NULL
    if (endpoint_ != NULL)
        launch_child (endpoint_);

    _endpoints.insert (std::make_pair (endpoint_pair_.identifier (),
                                       endpoint_pipe_t (endpoint_, pipe_)));

    if (pipe_ != NULL)
        pipe_->set_endpoint_pair (endpoint_pair_);
}

int slk::socket_base_t::term_endpoint (const char *endpoint_uri_)
{
    // Check whether the context hasn't been shut down yet
    if (unlikely (_ctx_terminated)) {
        errno = ETERM;
        return -1;
    }

    // Check whether endpoint address passed to the function is valid
    if (unlikely (!endpoint_uri_)) {
        errno = EINVAL;
        return -1;
    }

    // Process pending commands, if any
    const int rc = process_commands (0, false);
    if (unlikely (rc != 0)) {
        return -1;
    }

    // Parse endpoint_uri_ string
    std::string uri_protocol;
    std::string uri_path;
    if (parse_uri (endpoint_uri_, uri_protocol, uri_path)
        || check_protocol (uri_protocol)) {
        return -1;
    }

    const std::string endpoint_uri_str = std::string (endpoint_uri_);

    const std::string resolved_endpoint_uri =
      uri_protocol == slk::protocol_name::tcp
        ? resolve_tcp_addr (endpoint_uri_str, uri_path.c_str ())
        : endpoint_uri_str;

    // Find the endpoints range (if any) corresponding to the endpoint_uri_
    const std::pair<endpoints_t::iterator, endpoints_t::iterator> range =
      _endpoints.equal_range (resolved_endpoint_uri);
    if (range.first == range.second) {
        errno = ENOENT;
        return -1;
    }

    for (endpoints_t::iterator it = range.first; it != range.second; ++it) {
        // If we have an associated pipe, terminate it
        if (it->second.second != NULL)
            it->second.second->terminate (false);
        term_child (it->second.first);
    }
    _endpoints.erase (range.first, range.second);

    if (options.reconnect_stop & SL_RECONNECT_STOP_AFTER_DISCONNECT) {
        _disconnected = true;
    }

    return 0;
}

int slk::socket_base_t::send (msg_t *msg_, int flags_)
{
    // Check whether the context hasn't been shut down yet
    if (unlikely (_ctx_terminated)) {
        errno = ETERM;
        return -1;
    }

    // Check whether message passed to the function is valid
    if (unlikely (!msg_ || !msg_->check ())) {
        errno = EFAULT;
        return -1;
    }

    // Process pending commands, if any.
    int rc = process_commands (0, true);
    if (unlikely (rc != 0)) {
        return -1;
    }

    // Clear any user-visible flags that are set on the message
    msg_->reset_flags (msg_t::more);

    // At this point we impose the flags on the message
    if (flags_ & SL_SNDMORE)
        msg_->set_flags (msg_t::more);

    msg_->reset_metadata ();

    // Try to send the message using method in each socket class
    rc = xsend (msg_);
    if (rc == 0) {
        return 0;
    }
    if (unlikely (errno != EAGAIN)) {
        return -1;
    }

    // In case of non-blocking send we'll simply propagate
    // the error - including EAGAIN - up the stack
    if ((flags_ & SL_DONTWAIT) || options.sndtimeo == 0) {
        return -1;
    }

    // Compute the time when the timeout should occur
    // If the timeout is infinite, don't care
    int timeout = options.sndtimeo;
    const uint64_t end = timeout < 0 ? 0 : (_clock.now_ms () + timeout);

    // Oops, we couldn't send the message. Wait for the next
    // command, process it and try to send the message again
    // If timeout is reached in the meantime, return EAGAIN
    while (true) {
        if (unlikely (process_commands (timeout, false) != 0)) {
            return -1;
        }
        rc = xsend (msg_);
        if (rc == 0)
            break;
        if (unlikely (errno != EAGAIN)) {
            return -1;
        }
        if (timeout > 0) {
            timeout = static_cast<int> (end - _clock.now_ms ());
            if (timeout <= 0) {
                errno = EAGAIN;
                return -1;
            }
        }
    }

    return 0;
}

int slk::socket_base_t::recv (msg_t *msg_, int flags_)
{
    // Check whether the context hasn't been shut down yet
    if (unlikely (_ctx_terminated)) {
        errno = ETERM;
        return -1;
    }

    // Check whether message passed to the function is valid
    if (unlikely (!msg_ || !msg_->check ())) {
        errno = EFAULT;
        return -1;
    }

    // Once every inbound_poll_rate messages check for signals and process
    // incoming commands
    if (++_ticks == inbound_poll_rate) {
        if (unlikely (process_commands (0, false) != 0)) {
            return -1;
        }
        _ticks = 0;
    }

    // Get the message
    int rc = xrecv (msg_);
    if (unlikely (rc != 0 && errno != EAGAIN)) {
        return -1;
    }

    // If we have the message, return immediately
    if (rc == 0) {
        extract_flags (msg_);
        return 0;
    }

    // If the message cannot be fetched immediately, there are two scenarios
    // For non-blocking recv, commands are processed in case there's an
    // activate_reader command already waiting in a command pipe
    if ((flags_ & SL_DONTWAIT) || options.rcvtimeo == 0) {
        if (unlikely (process_commands (0, false) != 0)) {
            return -1;
        }
        _ticks = 0;

        rc = xrecv (msg_);
        if (rc < 0) {
            return rc;
        }
        extract_flags (msg_);

        return 0;
    }

    // Compute the time when the timeout should occur
    // If the timeout is infinite, don't care
    int timeout = options.rcvtimeo;
    const uint64_t end = timeout < 0 ? 0 : (_clock.now_ms () + timeout);

    // In blocking scenario, commands are processed over and over again until
    // we are able to fetch a message
    bool block = (_ticks != 0);
    while (true) {
        if (unlikely (process_commands (block ? timeout : 0, false) != 0)) {
            return -1;
        }
        rc = xrecv (msg_);
        if (rc == 0) {
            _ticks = 0;
            break;
        }
        if (unlikely (errno != EAGAIN)) {
            return -1;
        }
        block = true;
        if (timeout > 0) {
            timeout = static_cast<int> (end - _clock.now_ms ());
            if (timeout <= 0) {
                errno = EAGAIN;
                return -1;
            }
        }
    }

    extract_flags (msg_);
    return 0;
}

int slk::socket_base_t::close ()
{
    // Mark the socket as dead
    _tag = 0xdeadbeef;

    // Transfer the ownership of the socket from this application thread
    // to the reaper thread which will take care of the rest of shutdown
    // process
    send_reap (this);

    return 0;
}

bool slk::socket_base_t::has_in ()
{
    return xhas_in ();
}

bool slk::socket_base_t::has_out ()
{
    return xhas_out ();
}

void slk::socket_base_t::start_reaping (poller_t *poller_)
{
    // Plug the socket to the reaper thread
    _poller = poller_;

    fd_t fd;

    if (!_thread_safe)
        fd = (static_cast<mailbox_t *> (_mailbox))->get_fd ();
    else {
        slk_assert (false); // Thread-safe not implemented
    }

    _handle = _poller->add_fd (fd, this);
    _poller->set_pollin (_handle);

    // Initialise the termination and check whether it can be deallocated
    // immediately
    terminate ();
    check_destroy ();
}

int slk::socket_base_t::process_commands (int timeout_, bool throttle_)
{
    if (timeout_ == 0) {
        // If we are asked not to wait, check whether we haven't processed
        // commands recently, so that we can throttle the new commands

        // Get the CPU's tick counter. If 0, the counter is not available
        const uint64_t tsc = clock_t::rdtsc ();

        // Optimised version of command processing - it doesn't have to check
        // for incoming commands each time
        if (tsc && throttle_) {
            // Check whether TSC haven't jumped backwards (in case of migration
            // between CPU cores) and whether certain time have elapsed since
            // last command processing
            if (tsc >= _last_tsc && tsc - _last_tsc <= max_command_delay)
                return 0;
            _last_tsc = tsc;
        }
    }

    // Check whether there are any commands pending for this thread
    command_t cmd;
    int rc = _mailbox->recv (&cmd, timeout_);

    if (rc != 0 && errno == EINTR)
        return -1;

    // Process all available commands
    while (rc == 0 || errno == EINTR) {
        if (rc == 0) {
            cmd.destination->process_command (cmd);
        }
        rc = _mailbox->recv (&cmd, 0);
    }

    slk_assert (errno == EAGAIN);

    if (_ctx_terminated) {
        errno = ETERM;
        return -1;
    }

    return 0;
}

void slk::socket_base_t::process_stop ()
{
    // Here, someone is trying to deallocate the socket while there are still
    // commands pending in the mailbox
    // Just mark the socket as terminated and wait for the reaper to collect it
    _ctx_terminated = true;
}

void slk::socket_base_t::process_bind (pipe_t *pipe_)
{
    attach_pipe (pipe_);
}

void slk::socket_base_t::process_term (int linger_)
{
    // Unregister all inproc endpoints associated with this socket
    // After this point, we can be sure that no new pipes will be attached
    unregister_endpoints (this);

    // Ask all attached pipes to terminate
    for (pipes_t::size_type i = 0; i != _pipes.size (); ++i)
        _pipes[i]->terminate (false);
    register_term_acks (_pipes.size ());

    // Continue the termination process immediately
    own_t::process_term (linger_);
}

void slk::socket_base_t::process_term_endpoint (std::string *endpoint_)
{
    term_endpoint (endpoint_->c_str ());
    delete endpoint_;
}

void slk::socket_base_t::process_destroy ()
{
    _destroyed = true;
}

void slk::socket_base_t::in_event ()
{
    // This function is invoked only once the socket is running in the context
    // of the reaper thread. Process any commands from other threads
    process_commands (0, false);

    // If the socket is thread safe we need to unsignal the reaper signaler
    if (_thread_safe)
        slk_assert (false); // Thread-safe not implemented

    check_destroy ();
}

void slk::socket_base_t::out_event ()
{
    slk_assert (false);
}

void slk::socket_base_t::timer_event (int)
{
    slk_assert (false);
}

void slk::socket_base_t::check_destroy ()
{
    // If the object was already marked as destroyed, finish the deallocation
    if (_destroyed) {
        // Remove the socket from the reaper's poller
        if (_poller) {
            _poller->rm_fd (_handle);
            _handle = static_cast<poller_t::handle_t> (NULL);
            _poller = NULL;
        }

        // Remove the socket from the context
        destroy_socket (this);

        // Notify the reaper that this socket has been reaped
        send_reaped ();

        // Deallocate the socket
        delete this;
    }
}

void slk::socket_base_t::read_activated (pipe_t *pipe_)
{
    xread_activated (pipe_);
}

void slk::socket_base_t::write_activated (pipe_t *pipe_)
{
    xwrite_activated (pipe_);
}

void slk::socket_base_t::hiccuped (pipe_t *pipe_)
{
    xhiccuped (pipe_);
}

void slk::socket_base_t::pipe_terminated (pipe_t *pipe_)
{
    // Notify the specific socket type about the pipe termination
    xpipe_terminated (pipe_);

    // Remove the pipe from the list of attached pipes and confirm its
    // termination if we are already shutting down
    _pipes.erase (pipe_);
    if (is_terminating ())
        unregister_term_ack ();
}

void slk::socket_base_t::extract_flags (const msg_t *msg_)
{
    // Test whether MORE flag is set
    _rcvmore = (msg_->flags () & msg_t::more) != 0;
}

void slk::socket_base_t::update_pipe_options (int option_)
{
    if (option_ == SL_SNDHWM || option_ == SL_RCVHWM) {
        for (pipes_t::size_type i = 0; i != _pipes.size (); ++i) {
            _pipes[i]->set_hwms (options.rcvhwm, options.sndhwm);
        }
    }
}

int slk::socket_base_t::xsetsockopt (int, const void *, size_t)
{
    errno = EINVAL;
    return -1;
}

int slk::socket_base_t::xgetsockopt (int, void *, size_t *)
{
    errno = EINVAL;
    return -1;
}

bool slk::socket_base_t::xhas_out ()
{
    return false;
}

int slk::socket_base_t::xsend (msg_t *)
{
    errno = ENOTSUP;
    return -1;
}

bool slk::socket_base_t::xhas_in ()
{
    return false;
}

int slk::socket_base_t::xrecv (msg_t *)
{
    errno = ENOTSUP;
    return -1;
}

void slk::socket_base_t::xread_activated (pipe_t *)
{
}

void slk::socket_base_t::xwrite_activated (pipe_t *)
{
}

void slk::socket_base_t::xhiccuped (pipe_t *)
{
}

bool slk::socket_base_t::is_disconnected () const
{
    return _disconnected;
}

// routing_socket_base_t implementation

slk::routing_socket_base_t::routing_socket_base_t (class ctx_t *parent_,
                                                     uint32_t tid_,
                                                     int sid_) :
    socket_base_t (parent_, tid_, sid_)
{
}

slk::routing_socket_base_t::~routing_socket_base_t ()
{
}

int slk::routing_socket_base_t::xsetsockopt (int option_,
                                              const void *optval_,
                                              size_t optvallen_)
{
    // SL_CONNECT_ROUTING_ID sets the routing ID to assign to the peer when connecting
    // This is different from SL_ROUTING_ID which sets our own identity
    if (option_ == SL_CONNECT_ROUTING_ID) {
        if (optval_ && optvallen_) {
            _connect_routing_id.assign (static_cast<const char *> (optval_),
                                        optvallen_);
            return 0;
        }
    }
    errno = EINVAL;
    return -1;
}

void slk::routing_socket_base_t::xwrite_activated (pipe_t *pipe_)
{
    const out_pipes_t::iterator end = _out_pipes.end ();
    for (out_pipes_t::iterator it = _out_pipes.begin (); it != end; ++it)
        if (it->second.pipe == pipe_) {
            slk_assert (!it->second.active);
            it->second.active = true;
            return;
        }
    slk_assert (false);
}

std::string slk::routing_socket_base_t::extract_connect_routing_id ()
{
    std::string res = SL_MOVE (_connect_routing_id);
    _connect_routing_id.clear ();
    return res;
}

bool slk::routing_socket_base_t::connect_routing_id_is_set () const
{
    return !_connect_routing_id.empty ();
}

void slk::routing_socket_base_t::add_out_pipe (blob_t routing_id_,
                                                pipe_t *pipe_)
{
    // Add the record into output pipes lookup table
    const out_pipe_t outpipe = {pipe_, true};
    const bool ok =
      _out_pipes.SL_MAP_INSERT_OR_EMPLACE (SL_MOVE (routing_id_), outpipe)
        .second;
    slk_assert (ok);
}

bool slk::routing_socket_base_t::has_out_pipe (const blob_t &routing_id_) const
{
    return _out_pipes.find (routing_id_) != _out_pipes.end ();
}

slk::routing_socket_base_t::out_pipe_t *
slk::routing_socket_base_t::lookup_out_pipe (const blob_t &routing_id_)
{
    out_pipes_t::iterator it = _out_pipes.find (routing_id_);
    return it == _out_pipes.end () ? NULL : &it->second;
}

const slk::routing_socket_base_t::out_pipe_t *
slk::routing_socket_base_t::lookup_out_pipe (const blob_t &routing_id_) const
{
    out_pipes_t::const_iterator it = _out_pipes.find (routing_id_);
    return it == _out_pipes.end () ? NULL : &it->second;
}

void slk::routing_socket_base_t::erase_out_pipe (const pipe_t *pipe_)
{
    const out_pipes_t::iterator end = _out_pipes.end ();
    for (out_pipes_t::iterator it = _out_pipes.begin (); it != end; ++it) {
        if (it->second.pipe == pipe_) {
            _out_pipes.erase (it);
            return;
        }
    }
}

slk::routing_socket_base_t::out_pipe_t
slk::routing_socket_base_t::try_erase_out_pipe (const blob_t &routing_id_)
{
    const out_pipes_t::iterator it = _out_pipes.find (routing_id_);
    if (it != _out_pipes.end ()) {
        const out_pipe_t res = it->second;
        _out_pipes.erase (it);
        return res;
    }
    const out_pipe_t null_pipe = {NULL, false};
    return null_pipe;
}
