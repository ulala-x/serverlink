/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "ctx.hpp"
#include "socket_base.hpp"
#include "../io/io_thread.hpp"
#include "../io/reaper.hpp"
#include "../pipe/pipe.hpp"
#include "../pubsub/pubsub_registry.hpp"
#include "../util/err.hpp"
#include "../msg/msg.hpp"
#include "../util/random.hpp"
#include <limits.h>
#include <new>
#include <string.h>
#include <stdio.h>

#define SL_CTX_TAG_VALUE_GOOD 0xabadcafe
#define SL_CTX_TAG_VALUE_BAD 0xdeadbeef

static int clipped_maxsocket (int max_requested_)
{
    if (max_requested_ >= slk::poller_t::max_fds ()
        && slk::poller_t::max_fds () != -1)
        // -1 because we need room for the reaper mailbox
        max_requested_ = slk::poller_t::max_fds () - 1;

    return max_requested_;
}

slk::ctx_t::ctx_t () :
    _tag (SL_CTX_TAG_VALUE_GOOD),
    _starting (true),
    _terminating (false),
    _reaper (NULL),
    _pubsub_registry (NULL),
    _max_sockets (clipped_maxsocket (SL_MAX_SOCKETS_DFLT)),
    _max_msgsz (INT_MAX),
    _io_thread_count (SL_IO_THREADS_DFLT),
    _blocky (true),
    _ipv6 (false),
    _zero_copy (true)
{
    // Initialise crypto library, if needed
    slk::random_open ();

    // Create pub/sub registry for introspection
    _pubsub_registry = new (std::nothrow) pubsub_registry_t ();
}

bool slk::ctx_t::check_tag () const
{
    return _tag == SL_CTX_TAG_VALUE_GOOD;
}

slk::ctx_t::~ctx_t ()
{
    // Check that there are no remaining _sockets
    // FIXME: Commented out because socket reaping is not fully implemented yet
    // slk_assert (_sockets.empty ());

    // Ask I/O threads to terminate. If stop signal wasn't sent to I/O
    // thread subsequent invocation of destructor would hang-up
    const io_threads_t::size_type io_threads_size = _io_threads.size ();
    for (io_threads_t::size_type i = 0; i != io_threads_size; i++) {
        _io_threads[i]->stop ();
    }

    // Wait till I/O threads actually terminate
    for (io_threads_t::size_type i = 0; i != io_threads_size; i++) {
        delete _io_threads[i];
    }

    // Deallocate the reaper thread object
    delete _reaper;

    // Deallocate pub/sub registry
    delete _pubsub_registry;

    // The mailboxes in _slots themselves were deallocated with their
    // corresponding io_thread/socket objects

    // De-initialise crypto library, if needed
    slk::random_close ();

    // Remove the tag, so that the object is considered dead
    _tag = SL_CTX_TAG_VALUE_BAD;
}

bool slk::ctx_t::valid () const
{
    return _term_mailbox.valid ();
}

int slk::ctx_t::terminate ()
{
    _slot_sync.lock ();

    const bool save_terminating = _terminating;
    _terminating = false;

    // Clear any pending inproc connections
    // We simply remove them from the list. The pipes will be cleaned up
    // when their owning sockets are destroyed.
    {
        scoped_lock_t locker (_endpoints_sync);
        _pending_connections.clear ();
    }
    _terminating = save_terminating;

    if (!_starting) {
        // Check whether termination was already underway, but interrupted and now
        // restarted
        const bool restarted = _terminating;
        _terminating = true;

        // First attempt to terminate the context
        if (!restarted) {
            // First send stop command to sockets so that any blocking calls
            // can be interrupted
            for (sockets_t::size_type i = 0, size = _sockets.size (); i != size;
                 i++) {
                _sockets[i]->stop ();
            }
            // Stop the reaper to trigger cleanup
            _reaper->stop ();
        }
        _slot_sync.unlock ();

        // Wait till reaper thread closes all the sockets
        command_t cmd;
        const int rc = _term_mailbox.recv (&cmd, -1);
        if (rc == -1 && errno == EINTR)
            return -1;
        errno_assert (rc == 0);
        slk_assert (cmd.type == command_t::done);
        _slot_sync.lock ();
        // FIXME: Commented out because socket reaping is not fully implemented yet
        // slk_assert (_sockets.empty ());
    }
    _slot_sync.unlock ();

    // Deallocate the resources
    delete this;

    return 0;
}

int slk::ctx_t::shutdown ()
{
    scoped_lock_t locker (_slot_sync);

    if (!_terminating) {
        _terminating = true;

        if (!_starting) {
            // Send stop command to sockets so that any blocking calls
            // can be interrupted. If there are no sockets we can ask reaper
            // thread to stop
            for (sockets_t::size_type i = 0, size = _sockets.size (); i != size;
                 i++) {
                _sockets[i]->stop ();
            }
            if (_sockets.empty ())
                _reaper->stop ();
        }
    }

    return 0;
}

int slk::ctx_t::set (int option_, const void *optval_, size_t optvallen_)
{
    const bool is_int = (optvallen_ == sizeof (int));
    int value = 0;
    if (is_int)
        memcpy (&value, optval_, sizeof (int));

    switch (option_) {
        case SL_MAX_SOCKETS:
            if (is_int && value >= 1 && value == clipped_maxsocket (value)) {
                scoped_lock_t locker (_opt_sync);
                _max_sockets = value;
                return 0;
            }
            break;

        case SL_IO_THREADS:
            if (is_int && value >= 0) {
                scoped_lock_t locker (_opt_sync);
                _io_thread_count = value;
                return 0;
            }
            break;

        case SL_IPV6:
            if (is_int && value >= 0) {
                scoped_lock_t locker (_opt_sync);
                _ipv6 = (value != 0);
                return 0;
            }
            break;

        case SL_BLOCKY:
            if (is_int && value >= 0) {
                scoped_lock_t locker (_opt_sync);
                _blocky = (value != 0);
                return 0;
            }
            break;

        case SL_MAX_MSGSZ:
            if (is_int && value >= 0) {
                scoped_lock_t locker (_opt_sync);
                _max_msgsz = value < INT_MAX ? value : INT_MAX;
                return 0;
            }
            break;

        case SL_ZERO_COPY_RECV:
            if (is_int && value >= 0) {
                scoped_lock_t locker (_opt_sync);
                _zero_copy = (value != 0);
                return 0;
            }
            break;

        default: {
            return thread_ctx_t::set (option_, optval_, optvallen_);
        }
    }

    errno = EINVAL;
    return -1;
}

int slk::ctx_t::get (int option_, void *optval_, const size_t *optvallen_)
{
    const bool is_int = (*optvallen_ == sizeof (int));
    int *value = static_cast<int *> (optval_);

    switch (option_) {
        case SL_MAX_SOCKETS:
            if (is_int) {
                scoped_lock_t locker (_opt_sync);
                *value = _max_sockets;
                return 0;
            }
            break;

        case SL_SOCKET_LIMIT:
            if (is_int) {
                *value = clipped_maxsocket (65535);
                return 0;
            }
            break;

        case SL_IO_THREADS:
            if (is_int) {
                scoped_lock_t locker (_opt_sync);
                *value = _io_thread_count;
                return 0;
            }
            break;

        case SL_IPV6:
            if (is_int) {
                scoped_lock_t locker (_opt_sync);
                *value = _ipv6;
                return 0;
            }
            break;

        case SL_BLOCKY:
            if (is_int) {
                scoped_lock_t locker (_opt_sync);
                *value = _blocky;
                return 0;
            }
            break;

        case SL_MAX_MSGSZ:
            if (is_int) {
                scoped_lock_t locker (_opt_sync);
                *value = _max_msgsz;
                return 0;
            }
            break;

        case SL_ZERO_COPY_RECV:
            if (is_int) {
                scoped_lock_t locker (_opt_sync);
                *value = _zero_copy;
                return 0;
            }
            break;

        case SL_MSG_T_SIZE:
            if (is_int) {
                *value = sizeof (msg_t);
                return 0;
            }
            break;

        default: {
            return thread_ctx_t::get (option_, optval_, optvallen_);
        }
    }

    errno = EINVAL;
    return -1;
}

int slk::ctx_t::get (int option_)
{
    int optval = 0;
    size_t optvallen = sizeof (int);

    if (get (option_, &optval, &optvallen) == 0)
        return optval;

    errno = EINVAL;
    return -1;
}

bool slk::ctx_t::start ()
{
    // Initialise the array of mailboxes. Additional two slots are for
    // slk_ctx_term thread and reaper thread
    _opt_sync.lock ();
    const int term_and_reaper_threads_count = 2;
    const int mazsocks = _max_sockets;
    const int ios = _io_thread_count;
    _opt_sync.unlock ();
    const int slot_count = mazsocks + ios + term_and_reaper_threads_count;
    try {
        _slots.reserve (slot_count);
        _empty_slots.reserve (slot_count - term_and_reaper_threads_count);
    }
    catch (const std::bad_alloc &) {
        errno = ENOMEM;
        return false;
    }
    _slots.resize (term_and_reaper_threads_count);

    // Initialise the infrastructure for slk_ctx_term thread
    _slots[term_tid] = &_term_mailbox;

    // Create the reaper thread
    _reaper = new (std::nothrow) reaper_t (this, reaper_tid);
    if (!_reaper) {
        errno = ENOMEM;
        goto fail_cleanup_slots;
    }
    if (!_reaper->get_mailbox ()->valid ())
        goto fail_cleanup_reaper;
    _slots[reaper_tid] = _reaper->get_mailbox ();
    _reaper->start ();

    // Create I/O thread objects and launch them
    _slots.resize (slot_count, NULL);

    for (int i = term_and_reaper_threads_count;
         i != ios + term_and_reaper_threads_count; i++) {
        io_thread_t *io_thread = new (std::nothrow) io_thread_t (this, i);
        if (!io_thread) {
            errno = ENOMEM;
            goto fail_cleanup_reaper;
        }
        if (!io_thread->get_mailbox ()->valid ()) {
            delete io_thread;
            goto fail_cleanup_reaper;
        }
        _io_threads.push_back (io_thread);
        _slots[i] = io_thread->get_mailbox ();
        io_thread->start ();
    }

    // In the unused part of the slot array, create a list of empty slots
    for (int32_t i = static_cast<int32_t> (_slots.size ()) - 1;
         i >= static_cast<int32_t> (ios) + term_and_reaper_threads_count; i--) {
        _empty_slots.push_back (i);
    }

    _starting = false;
    return true;

fail_cleanup_reaper:
    _reaper->stop ();
    delete _reaper;
    _reaper = NULL;

fail_cleanup_slots:
    _slots.clear ();
    return false;
}

slk::socket_base_t *slk::ctx_t::create_socket (int type_)
{
    scoped_lock_t locker (_slot_sync);

    // Once slk_ctx_term() or slk_ctx_shutdown() was called, we can't create
    // new sockets
    if (_terminating) {
        errno = ETERM;
        return NULL;
    }

    if (unlikely (_starting)) {
        if (!start ())
            return NULL;
    }

    // If max_sockets limit was reached, return error
    if (_empty_slots.empty ()) {
        errno = EMFILE;
        return NULL;
    }

    // Choose a slot for the socket
    const uint32_t slot = _empty_slots.back ();
    _empty_slots.pop_back ();

    // Generate new unique socket ID
    const int sid = (static_cast<int> (max_socket_id.add (1))) + 1;

    // Create the socket and register its mailbox
    socket_base_t *s = socket_base_t::create (type_, this, slot, sid);
    if (!s) {
        _empty_slots.push_back (slot);
        return NULL;
    }
    _sockets.push_back (s);
    _slots[slot] = s->get_mailbox ();

    return s;
}

void slk::ctx_t::destroy_socket (class socket_base_t *socket_)
{
    scoped_lock_t locker (_slot_sync);

    // Free the associated thread slot
    const uint32_t tid = socket_->get_tid ();
    _empty_slots.push_back (tid);
    _slots[tid] = NULL;

    // Remove the socket from the list of sockets
    _sockets.erase (socket_);

    // If slk_ctx_term() was already called and there are no more socket
    // we can ask reaper thread to terminate
    if (_terminating && _sockets.empty ())
        _reaper->stop ();
}

slk::object_t *slk::ctx_t::get_reaper () const
{
    return _reaper;
}

slk::pubsub_registry_t *slk::ctx_t::get_pubsub_registry () const
{
    return _pubsub_registry;
}

slk::thread_ctx_t::thread_ctx_t () :
    _thread_priority (SL_THREAD_PRIORITY_DFLT),
    _thread_sched_policy (SL_THREAD_SCHED_POLICY_DFLT)
{
}

void slk::thread_ctx_t::start_thread (thread_t &thread_,
                                      thread_fn *tfn_,
                                      void *arg_,
                                      const char *name_) const
{
    thread_.setSchedulingParameters (_thread_priority, _thread_sched_policy,
                                     _thread_affinity_cpus);

    char namebuf[16] = "";
    snprintf (namebuf, sizeof (namebuf), "%s%sSLbg%s%s",
              _thread_name_prefix.empty () ? "" : _thread_name_prefix.c_str (),
              _thread_name_prefix.empty () ? "" : "/", name_ ? "/" : "",
              name_ ? name_ : "");
    thread_.start (tfn_, arg_, namebuf);
}

int slk::thread_ctx_t::set (int option_, const void *optval_, size_t optvallen_)
{
    const bool is_int = (optvallen_ == sizeof (int));
    int value = 0;
    if (is_int)
        memcpy (&value, optval_, sizeof (int));

    switch (option_) {
        case SL_THREAD_SCHED_POLICY:
            if (is_int && value >= 0) {
                scoped_lock_t locker (_opt_sync);
                _thread_sched_policy = value;
                return 0;
            }
            break;

        case SL_THREAD_AFFINITY_CPU_ADD:
            if (is_int && value >= 0) {
                scoped_lock_t locker (_opt_sync);
                _thread_affinity_cpus.insert (value);
                return 0;
            }
            break;

        case SL_THREAD_AFFINITY_CPU_REMOVE:
            if (is_int && value >= 0) {
                scoped_lock_t locker (_opt_sync);
                if (0 == _thread_affinity_cpus.erase (value)) {
                    errno = EINVAL;
                    return -1;
                }
                return 0;
            }
            break;

        case SL_THREAD_PRIORITY:
            if (is_int && value >= 0) {
                scoped_lock_t locker (_opt_sync);
                _thread_priority = value;
                return 0;
            }
            break;

        case SL_THREAD_NAME_PREFIX:
            if (optvallen_ > 0 && optvallen_ <= 16) {
                scoped_lock_t locker (_opt_sync);
                _thread_name_prefix.assign (static_cast<const char *> (optval_),
                                            optvallen_);
                return 0;
            }
            break;
    }

    errno = EINVAL;
    return -1;
}

int slk::thread_ctx_t::get (int option_,
                            void *optval_,
                            const size_t *optvallen_)
{
    const bool is_int = (*optvallen_ == sizeof (int));
    int *value = static_cast<int *> (optval_);

    switch (option_) {
        case SL_THREAD_SCHED_POLICY:
            if (is_int) {
                scoped_lock_t locker (_opt_sync);
                *value = _thread_sched_policy;
                return 0;
            }
            break;

        case SL_THREAD_PRIORITY:
            if (is_int) {
                scoped_lock_t locker (_opt_sync);
                *value = _thread_priority;
                return 0;
            }
            break;

        case SL_THREAD_NAME_PREFIX:
            if (*optvallen_ >= _thread_name_prefix.size ()) {
                scoped_lock_t locker (_opt_sync);
                memcpy (optval_, _thread_name_prefix.data (),
                        _thread_name_prefix.size ());
                return 0;
            }
            break;
    }

    errno = EINVAL;
    return -1;
}

void slk::ctx_t::send_command (uint32_t tid_, const command_t &command_)
{
    _slots[tid_]->send (command_);
}

slk::io_thread_t *slk::ctx_t::choose_io_thread (uint64_t affinity_)
{
    if (_io_threads.empty ())
        return NULL;

    // Find the I/O thread with minimum load
    int min_load = -1;
    io_thread_t *selected_io_thread = NULL;
    for (io_threads_t::size_type i = 0, size = _io_threads.size (); i != size;
         i++) {
        if (!affinity_ || (affinity_ & (uint64_t (1) << i))) {
            const int load = _io_threads[i]->get_load ();
            if (selected_io_thread == NULL || load < min_load) {
                min_load = load;
                selected_io_thread = _io_threads[i];
            }
        }
    }
    return selected_io_thread;
}

int slk::ctx_t::register_endpoint (const char *addr_,
                                   const endpoint_t &endpoint_)
{
    scoped_lock_t locker (_endpoints_sync);

    const bool inserted =
      _endpoints.insert (std::make_pair (std::string (addr_), endpoint_)).second;
    if (!inserted) {
        errno = EADDRINUSE;
        return -1;
    }
    return 0;
}

int slk::ctx_t::unregister_endpoint (const std::string &addr_,
                                     const socket_base_t *const socket_)
{
    scoped_lock_t locker (_endpoints_sync);

    const endpoints_t::iterator it = _endpoints.find (addr_);
    if (it == _endpoints.end () || it->second.socket != socket_) {
        errno = ENOENT;
        return -1;
    }

    // Remove endpoint
    _endpoints.erase (it);

    return 0;
}

void slk::ctx_t::unregister_endpoints (const socket_base_t *const socket_)
{
    scoped_lock_t locker (_endpoints_sync);

    for (endpoints_t::iterator it = _endpoints.begin (),
                               end = _endpoints.end ();
         it != end;) {
        if (it->second.socket == socket_)
            it = _endpoints.erase (it);
        else
            ++it;
    }
}

slk::endpoint_t slk::ctx_t::find_endpoint (const char *addr_)
{
    scoped_lock_t locker (_endpoints_sync);

    endpoints_t::iterator it = _endpoints.find (addr_);
    if (it == _endpoints.end ()) {
        errno = ECONNREFUSED;
        endpoint_t empty = {NULL, options_t ()};
        return empty;
    }
    endpoint_t endpoint = it->second;

    // Increment the command sequence number of the peer so that it won't
    // get deallocated until "bind" command is issued by the caller
    // The subsequent 'bind' has to be called with inc_seqnum parameter
    // set to false, so that the seqnum isn't incremented twice
    endpoint.socket->inc_seqnum ();

    return endpoint;
}

void slk::ctx_t::pend_connection (const std::string &addr_,
                                  const endpoint_t &endpoint_,
                                  pipe_t **pipes_)
{
    scoped_lock_t locker (_endpoints_sync);

    const pending_connection_t pending_connection = {endpoint_, pipes_[0],
                                                     pipes_[1]};

    const endpoints_t::iterator it = _endpoints.find (addr_);
    if (it == _endpoints.end ()) {
        // Still no bind
        endpoint_.socket->inc_seqnum ();
        _pending_connections.insert (std::make_pair (addr_, pending_connection));
    } else {
        // Bind has happened in the mean time, connect directly
        connect_inproc_sockets (it->second.socket, it->second.options,
                                pending_connection, connect_side);
    }
}

void slk::ctx_t::connect_pending (const char *addr_,
                                  slk::socket_base_t *bind_socket_)
{
    scoped_lock_t locker (_endpoints_sync);

    const std::pair<pending_connections_t::iterator,
                    pending_connections_t::iterator>
      pending = _pending_connections.equal_range (addr_);
    for (pending_connections_t::iterator p = pending.first; p != pending.second;
         ++p)
        connect_inproc_sockets (bind_socket_, _endpoints[addr_].options,
                                p->second, bind_side);

    _pending_connections.erase (pending.first, pending.second);
}

void slk::ctx_t::connect_inproc_sockets (
  slk::socket_base_t *bind_socket_,
  const options_t &bind_options_,
  const pending_connection_t &pending_connection_,
  side side_)
{
    // Increment sequence number for bind socket
    bind_socket_->inc_seqnum ();

    // Set the thread ID for bind pipe
    pending_connection_.bind_pipe->set_tid (bind_socket_->get_tid ());

    // If the connect socket does not want to receive routing IDs,
    // read and discard the routing ID message from bind pipe
    if (!bind_options_.recv_routing_id) {
        msg_t msg;
        const bool ok = pending_connection_.bind_pipe->read (&msg);
        slk_assert (ok);
        const int rc = msg.close ();
        errno_assert (rc == 0);
    }

    // ServerLink only supports ROUTER sockets, so conflate is not effective.
    // Always set HWMs with boost for inproc connections.
    pending_connection_.connect_pipe->set_hwms_boost (bind_options_.sndhwm,
                                                      bind_options_.rcvhwm);
    pending_connection_.bind_pipe->set_hwms_boost (
      pending_connection_.endpoint.options.sndhwm,
      pending_connection_.endpoint.options.rcvhwm);

    pending_connection_.connect_pipe->set_hwms (
      pending_connection_.endpoint.options.rcvhwm,
      pending_connection_.endpoint.options.sndhwm);
    pending_connection_.bind_pipe->set_hwms (bind_options_.rcvhwm,
                                             bind_options_.sndhwm);

    // Send bind command and inproc_connected signal based on which side initiated
    if (side_ == bind_side) {
        // Bind was called after connect
        command_t cmd;
        cmd.type = command_t::bind;
        cmd.args.bind.pipe = pending_connection_.bind_pipe;
        bind_socket_->process_command (cmd);
        bind_socket_->send_inproc_connected (
          pending_connection_.endpoint.socket);
    } else {
        // Connect was called after bind (normal case)
        pending_connection_.connect_pipe->send_bind (
          bind_socket_, pending_connection_.bind_pipe, false);
    }

    // When a ctx is terminated all pending inproc connection will be
    // connected, but the socket will already be closed and the pipe will be
    // in waiting_for_delimiter state, which means no more writes can be done
    // and the routing id write fails and causes an assert. Check if the socket
    // is open before sending.
    if (pending_connection_.endpoint.options.recv_routing_id
        && pending_connection_.endpoint.socket->check_tag ()) {
        send_routing_id (pending_connection_.bind_pipe, bind_options_);
    }
}

// The last used socket ID, or 0 if no socket was used so far. Note that this
// is a global variable. Thus, even sockets created in different contexts have
// unique IDs
slk::atomic_counter_t slk::ctx_t::max_socket_id;
