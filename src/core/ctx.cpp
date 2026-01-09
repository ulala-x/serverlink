/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "../precompiled.hpp"
#include "ctx.hpp"
#include "socket_base.hpp"
#include "../io/io_thread.hpp"
#include "../io/reaper.hpp"
#include "../pipe/pipe.hpp"
#include "../util/err.hpp"
#include "../util/random.hpp"
#include "../util/likely.hpp"

#include <new>
#include <string.h>
#include <limits.h>

slk::atomic_counter_t slk::ctx_t::max_socket_id;

slk::ctx_t::ctx_t () :
    _tag (0xbaddecaf),
    _starting (true),
    _terminating (false),
    _reaper (NULL),
    _max_sockets (SL_MAX_SOCKETS_DFLT),
    _max_msgsz (INT_MAX),
    _io_thread_count (SL_IO_THREADS_DFLT),
    _blocky (true),
    _ipv6 (false),
    _zero_copy (false)
{
}

bool slk::ctx_t::check_tag () const
{
    return _tag == 0xbaddecaf;
}

bool slk::ctx_t::valid () const
{
    return check_tag();
}

slk::ctx_t::~ctx_t ()
{
    _endpoints.clear();

    if (_reaper)
        delete _reaper;

    _tag = 0xdeadbeef;
}

int slk::ctx_t::terminate ()
{
    scoped_lock_t locker (_slot_sync);

    if (_terminating) {
        return 0;
    }

    _terminating = true;

    if (_sockets.empty ()) {
        if (_reaper)
            _reaper->stop ();
    }

    return 0;
}

int slk::ctx_t::shutdown ()
{
    scoped_lock_t locker (_slot_sync);
    _terminating = true;
    return 0;
}

int slk::ctx_t::set (int option_, const void *optval_, size_t optvallen_)
{
    scoped_lock_t locker (_slot_sync);
    if (_terminating) {
        errno = ETERM;
        return -1;
    }

    switch (option_) {
        case SL_MAX_SOCKETS:
            if (optvallen_ != sizeof (int) || *((int *) optval_) < 1) {
                errno = EINVAL;
                return -1;
            }
            _max_sockets = *((int *) optval_);
            return 0;
        case SL_IO_THREADS:
            if (optvallen_ != sizeof (int) || *((int *) optval_) < 0) {
                errno = EINVAL;
                return -1;
            }
            _io_thread_count = *((int *) optval_);
            return 0;
        case SL_BLOCKY:
            if (optvallen_ != sizeof (int)) {
                errno = EINVAL;
                return -1;
            }
            _blocky = (*((int *) optval_) != 0);
            return 0;
    }

    return thread_ctx_t::set (option_, optval_, optvallen_);
}

int slk::ctx_t::get (int option_, void *optval_, const size_t *optvallen_)
{
    scoped_lock_t locker (_slot_sync);
    switch (option_) {
        case SL_MAX_SOCKETS:
            if (*optvallen_ != sizeof (int)) return -1;
            *((int *) optval_) = _max_sockets;
            return 0;
        case SL_IO_THREADS:
            if (*optvallen_ != sizeof (int)) return -1;
            *((int *) optval_) = _io_thread_count;
            return 0;
        case SL_SOCKET_LIMIT:
            if (*optvallen_ != sizeof (int)) return -1;
            *((int *) optval_) = _max_sockets;
            return 0;
    }
    return thread_ctx_t::get(option_, optval_, optvallen_);
}

int slk::ctx_t::get (int option_)
{
    switch (option_) {
        case SL_MAX_SOCKETS:
            return _max_sockets;
        case SL_IO_THREADS:
            return _io_thread_count;
        case SL_IPV6:
            return _ipv6 ? 1 : 0;
        case SL_BLOCKY:
            return _blocky ? 1 : 0;
    }
    return -1;
}

slk::socket_base_t *slk::ctx_t::create_socket (int type_)
{
    scoped_lock_t locker (_slot_sync);

    if (_terminating) {
        errno = ETERM;
        return NULL;
    }

    if (unlikely (_starting)) {
        if (!start ())
            return NULL;
    }

    if (_empty_slots.empty ()) {
        errno = EMFILE;
        return NULL;
    }

    const uint32_t slot = _empty_slots.back ();
    _empty_slots.pop_back ();

    const int sid = (static_cast<int> (max_socket_id.add (1))) + 1;

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

    const uint32_t tid = socket_->get_tid ();
    _empty_slots.push_back (tid);
    _slots[tid] = NULL;

    _sockets.erase (socket_);

    if (_terminating && _sockets.empty ()) {
        if (_reaper) _reaper->stop ();
    }
}

slk::io_thread_t *slk::ctx_t::choose_io_thread (uint64_t affinity_)
{
    if (_io_threads.empty ())
        return NULL;

    int min_load = -1;
    io_thread_t *selected_io_thread = NULL;
    for (io_threads_t::size_type i = 0, size = _io_threads.size (); i != size; i++) {
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

bool slk::ctx_t::start ()
{
    if (!_starting) return true;

    _slots.resize (_io_thread_count + 2, NULL);
    _slots[term_tid] = &_term_mailbox;

    for (int i = 0; i != _io_thread_count; i++) {
        io_thread_t *io_thread = new (std::nothrow) io_thread_t (this, i + 2);
        alloc_assert (io_thread);
        _io_threads.push_back (io_thread);
        _slots[i + 2] = io_thread->get_mailbox ();
    }

    _reaper = new (std::nothrow) reaper_t (this, reaper_tid);
    alloc_assert (_reaper);
    _slots[reaper_tid] = _reaper->get_mailbox ();

    for (auto it : _io_threads) it->start ();
    _reaper->start ();

    for (uint32_t i = (uint32_t)_slots.size(); i < 1024; i++) {
        _empty_slots.push_back(i);
    }
    _slots.resize(1024, NULL);

    _starting = false;
    return true;
}

slk::object_t *slk::ctx_t::get_reaper () const { return (object_t*)_reaper; }

void slk::ctx_t::send_command (uint32_t tid_, const command_t &command_)
{
    slk_assert (tid_ < _slots.size ());
    i_mailbox *slot = _slots[tid_];
    slk_assert (slot);
    slot->send (command_);
}

int slk::ctx_t::register_endpoint (const char *addr_, const endpoint_t &endpoint_)
{
    scoped_lock_t locker (_endpoints_sync);
    _endpoints[addr_] = endpoint_;
    return 0;
}

int slk::ctx_t::unregister_endpoint (const std::string &addr_, const socket_base_t *socket_)
{
    scoped_lock_t locker (_endpoints_sync);
    auto it = _endpoints.find(addr_);
    if (it != _endpoints.end() && it->second.socket == socket_) {
        _endpoints.erase(it);
        return 0;
    }
    return -1;
}

slk::endpoint_t slk::ctx_t::find_endpoint (const char *addr_)
{
    scoped_lock_t locker (_endpoints_sync);
    auto it = _endpoints.find(addr_);
    if (it == _endpoints.end()) return endpoint_t();
    return it->second;
}

void slk::ctx_t::unregister_endpoints (const slk::socket_base_t *socket_)
{
    scoped_lock_t locker (_endpoints_sync);
    for (auto it = _endpoints.begin(); it != _endpoints.end(); ) {
        if (it->second.socket == socket_) {
            _endpoints.erase(it++);
        }
        else ++it;
    }
}

void slk::ctx_t::pend_connection (const std::string &addr_,
                                  const endpoint_t &endpoint_,
                                  pipe_t **pipes_)
{
    scoped_lock_t locker (_endpoints_sync);
    pending_connection_t pending_connection;
    pending_connection.endpoint = endpoint_;
    pending_connection.connect_pipe = pipes_[0];
    pending_connection.bind_pipe = pipes_[1];
    _pending_connections.insert(std::make_pair(addr_, pending_connection));
}

void slk::ctx_t::connect_pending (const char *addr_, slk::socket_base_t *bind_socket_)
{
    scoped_lock_t locker (_endpoints_sync);
    auto range = _pending_connections.equal_range(addr_);
    for (auto it = range.first; it != range.second; ++it) {
        connect_inproc_sockets(bind_socket_, bind_socket_->options, it->second, bind_side);
    }
    _pending_connections.erase(range.first, range.second);
}

void slk::ctx_t::connect_inproc_sockets (slk::socket_base_t *bind_socket_,
                                         const options_t &bind_options_,
                                         const pending_connection_t &pending_connection_,
                                         side side_)
{
    // The critical implementation of inproc connection
    bind_socket_->inc_seqnum();
    pending_connection_.endpoint.socket->inc_seqnum();

    if (side_ == bind_side) {
        // Connect side is already pending, we are binding
        // We need to attach pipes to sockets
        // The pipes were created in pend_connection caller (socket_base::connect)
        // Actually, pipes are created by the connector/listener logic.
        // For inproc, socket_base manages this.
        
        // Wait, socket_base::connect_internal creates the pipes for inproc.
        // We just need to attach them.
        
        // This function is static but accesses private members of socket_base_t?
        // Yes, ctx_t is friend of socket_base_t.
        
        // Attach bind pipe to bind socket
        bind_socket_->attach_pipe(pending_connection_.bind_pipe, true, false);
        
        // Attach connect pipe to connect socket
        pending_connection_.endpoint.socket->attach_pipe(pending_connection_.connect_pipe, true, true);
    }
    else {
        // Connect side
        // But this function is only called from connect_pending (which is called from bind)
        // So side_ is always bind_side?
        // Wait, connect_inproc_sockets is also called if find_endpoint succeeds immediately.
        
        // If side_ == connect_side:
        // We found the bind socket immediately.
        // pending_connection_ contains the connect side info?
        // No, if called from connect_side, 'bind_socket_' is the bound socket we found.
        // 'pending_connection_' holds the connect side info we just created.
        
        bind_socket_->attach_pipe(pending_connection_.bind_pipe, true, false);
        pending_connection_.endpoint.socket->attach_pipe(pending_connection_.connect_pipe, true, true);
    }
}

slk::thread_ctx_t::thread_ctx_t () : _thread_priority (SL_THREAD_PRIORITY_DFLT), _thread_sched_policy (SL_THREAD_SCHED_POLICY_DFLT) {}
int slk::thread_ctx_t::set (int, const void*, size_t) { return 0; }
void slk::thread_ctx_t::start_thread (thread_t &t, thread_fn *f, void *a, const char *n) const { t.start(f, a, n); }
int slk::thread_ctx_t::get (int o, void* v, const size_t* l) { (void)o; (void)v; (void)l; return 0; }
