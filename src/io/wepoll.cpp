/* SPDX-License-Identifier: MPL-2.0 */

#include "../util/config.hpp"
#if defined SL_USE_WEPOLL

#include "wepoll.hpp"
#include "i_poll_events.hpp"
#include "../util/err.hpp"
#include "../util/macros.hpp"

#include <algorithm>
#include <new>
#include <cstdio>
#include <cstdlib>

// Windows-specific error checking macro
#define wsa_assert(x)                                                          \
    do {                                                                       \
        if (!(x)) {                                                            \
            const char *errstr = slk::wsa_error ();                            \
            if (errstr != nullptr) {                                           \
                std::fprintf (stderr, "%s (%s:%d)\n", errstr, __FILE__, __LINE__); \
            }                                                                  \
            std::fflush (stderr);                                              \
            std::abort ();                                                     \
        }                                                                      \
    } while (false)

namespace slk
{
wepoll_t::wepoll_t (ctx_t *ctx_) : worker_poller_base_t (ctx_)
{
    // No initialization needed - events are created per-socket
}

wepoll_t::~wepoll_t ()
{
    // Wait till the worker thread exits
    stop_worker ();

    // Clean up all poll entries and their event objects
    for (poll_entries_t::iterator it = _entries.begin (), end = _entries.end ();
         it != end; ++it) {
        if (*it != nullptr) {
            if ((*it)->event != WSA_INVALID_EVENT) {
                // Disassociate event from socket
                WSAEventSelect ((*it)->fd, (*it)->event, 0);
                WSACloseEvent ((*it)->event);
            }
            delete *it;
        }
    }

    // Clean up retired entries
    for (retired_t::iterator it = _retired.begin (), end = _retired.end ();
         it != end; ++it) {
        if (*it != nullptr) {
            if ((*it)->event != WSA_INVALID_EVENT) {
                WSAEventSelect ((*it)->fd, (*it)->event, 0);
                WSACloseEvent ((*it)->event);
            }
            delete *it;
        }
    }
}

wepoll_t::handle_t wepoll_t::add_fd (fd_t fd_, i_poll_events *events_)
{
    check_thread ();
    poll_entry_t *pe = new (std::nothrow) poll_entry_t;
    alloc_assert (pe);

    // Initialize poll entry
    pe->fd = fd_;
    pe->events = events_;
    pe->pollin = false;
    pe->pollout = false;

    // Create Windows event object for this socket
    pe->event = WSACreateEvent ();
    wsa_assert (pe->event != WSA_INVALID_EVENT);

    _entries.push_back (pe);

    // Increase the load metric of the thread
    adjust_load (1);

    return pe;
}

void wepoll_t::rm_fd (handle_t handle_)
{
    check_thread ();
    poll_entry_t *pe = static_cast<poll_entry_t *> (handle_);

    // Disassociate event from socket
    if (pe->event != WSA_INVALID_EVENT) {
        const int rc = WSAEventSelect (pe->fd, pe->event, 0);
        wsa_assert (rc != SOCKET_ERROR);
    }

    // Mark as retired
    pe->fd = retired_fd;
    _retired.push_back (pe);

    // Decrease the load metric of the thread
    adjust_load (-1);
}

void wepoll_t::set_pollin (handle_t handle_)
{
    check_thread ();
    poll_entry_t *pe = static_cast<poll_entry_t *> (handle_);
    if (!pe->pollin) {
        pe->pollin = true;
        update_socket_events (pe);
    }
}

void wepoll_t::reset_pollin (handle_t handle_)
{
    check_thread ();
    poll_entry_t *pe = static_cast<poll_entry_t *> (handle_);
    if (pe->pollin) {
        pe->pollin = false;
        update_socket_events (pe);
    }
}

void wepoll_t::set_pollout (handle_t handle_)
{
    check_thread ();
    poll_entry_t *pe = static_cast<poll_entry_t *> (handle_);
    if (!pe->pollout) {
        pe->pollout = true;
        update_socket_events (pe);
    }
}

void wepoll_t::reset_pollout (handle_t handle_)
{
    check_thread ();
    poll_entry_t *pe = static_cast<poll_entry_t *> (handle_);
    if (pe->pollout) {
        pe->pollout = false;
        update_socket_events (pe);
    }
}

void wepoll_t::stop ()
{
    check_thread ();
    _stopping = true;
}

int wepoll_t::max_fds ()
{
    // No hard limit like select's FD_SETSIZE
    // Limited only by system resources and MAXIMUM_WAIT_OBJECTS batching
    return -1;
}

void wepoll_t::update_socket_events (poll_entry_t *pe_)
{
    if (pe_->fd == retired_fd)
        return;

    long events = FD_CLOSE;  // Always monitor for close events

    if (pe_->pollin)
        events |= FD_READ | FD_ACCEPT;

    if (pe_->pollout)
        events |= FD_WRITE | FD_CONNECT;

    // Associate the event object with the socket and specify event types
    const int rc = WSAEventSelect (pe_->fd, pe_->event, events);
    wsa_assert (rc != SOCKET_ERROR);
}

void wepoll_t::process_events (const std::vector<poll_entry_t *> &signaled_entries_)
{
    for (std::vector<poll_entry_t *>::const_iterator it = signaled_entries_.begin (),
                                                      end = signaled_entries_.end ();
         it != end; ++it) {
        poll_entry_t *pe = *it;

        if (pe->fd == retired_fd)
            continue;
        if (pe->events == nullptr)
            continue;

        // Get network events for this socket
        WSANETWORKEVENTS net_events;
        const int rc = WSAEnumNetworkEvents (pe->fd, pe->event, &net_events);
        wsa_assert (rc != SOCKET_ERROR);

        // Process error/close events first (highest priority)
        // FD_CLOSE or any error should trigger in_event for proper cleanup
        if ((net_events.lNetworkEvents & FD_CLOSE) ||
            net_events.iErrorCode[FD_READ_BIT] != 0 ||
            net_events.iErrorCode[FD_WRITE_BIT] != 0 ||
            net_events.iErrorCode[FD_ACCEPT_BIT] != 0 ||
            net_events.iErrorCode[FD_CONNECT_BIT] != 0) {
            pe->events->in_event ();
            if (pe->fd == retired_fd)
                continue;
        }

        // Process write events (FD_WRITE, FD_CONNECT)
        if (net_events.lNetworkEvents & (FD_WRITE | FD_CONNECT)) {
            pe->events->out_event ();
            if (pe->fd == retired_fd)
                continue;
        }

        // Process read events (FD_READ, FD_ACCEPT)
        if (net_events.lNetworkEvents & (FD_READ | FD_ACCEPT)) {
            pe->events->in_event ();
        }
    }
}

void wepoll_t::loop ()
{
    // Maximum number of events that can be waited on simultaneously
    // Windows limit is MAXIMUM_WAIT_OBJECTS (64)
    static constexpr DWORD max_events = MAXIMUM_WAIT_OBJECTS;

    std::vector<poll_entry_t *> signaled_entries;
    signaled_entries.reserve (max_events);

    while (!_stopping) {
        // Execute any due timers
        const uint64_t timeout_ms = execute_timers ();

        if (get_load () == 0) {
            if (timeout_ms == 0)
                break;
            // No sockets to monitor, just sleep for timer
            Sleep (static_cast<DWORD> (timeout_ms));
            continue;
        }

        // Collect active (non-retired) entries
        std::vector<poll_entry_t *> active_entries;
        active_entries.reserve (_entries.size ());
        for (poll_entries_t::iterator it = _entries.begin (), end = _entries.end ();
             it != end; ++it) {
            if ((*it)->fd != retired_fd) {
                active_entries.push_back (*it);
            }
        }

        if (active_entries.empty ()) {
            if (timeout_ms == 0)
                break;
            Sleep (static_cast<DWORD> (timeout_ms));
            continue;
        }

        // Process sockets in batches if we exceed MAXIMUM_WAIT_OBJECTS
        const size_t total_sockets = active_entries.size ();
        size_t batch_start = 0;

        while (batch_start < total_sockets && !_stopping) {
            // Determine batch size
            // Use parentheses around std::min to avoid Windows max/min macro conflict
            const size_t batch_size =
                (std::min) (static_cast<size_t> (max_events),
                            total_sockets - batch_start);

            // Build array of event handles for this batch
            WSAEVENT event_array[max_events];
            for (size_t i = 0; i < batch_size; ++i) {
                event_array[i] = active_entries[batch_start + i]->event;
            }

            // Wait for events
            // If processing multiple batches, only use timeout on first batch
            const DWORD wait_timeout =
                (batch_start == 0 && timeout_ms > 0)
                    ? static_cast<DWORD> (timeout_ms)
                    : 0;

            const DWORD result = WSAWaitForMultipleEvents (
                static_cast<DWORD> (batch_size),
                event_array,
                FALSE,  // Wait for any event, not all
                wait_timeout,
                FALSE   // Not alertable
            );

            if (result == WSA_WAIT_TIMEOUT) {
                // Timeout - no events signaled in this batch
                batch_start += batch_size;
                continue;
            }

            if (result == WSA_WAIT_FAILED) {
                wsa_assert (false);
                break;
            }

            // Calculate which event was signaled
            const DWORD index = result - WSA_WAIT_EVENT_0;

            if (index < batch_size) {
                // Single event was signaled - process it
                signaled_entries.clear ();
                signaled_entries.push_back (active_entries[batch_start + index]);
                process_events (signaled_entries);
            } else {
                // This shouldn't happen, but handle gracefully
                wsa_assert (false);
            }

            batch_start += batch_size;
        }

        // Destroy retired event sources
        for (retired_t::iterator it = _retired.begin (), end = _retired.end ();
             it != end; ++it) {
            poll_entry_t *pe = *it;
            if (pe->event != WSA_INVALID_EVENT) {
                WSACloseEvent (pe->event);
            }
            delete pe;
        }
        _retired.clear ();

        // Remove retired entries from main list
        _entries.erase (
            std::remove_if (_entries.begin (), _entries.end (),
                            [](poll_entry_t *pe) { return pe->fd == retired_fd; }),
            _entries.end ());
    }
}

} // namespace slk

#endif // SL_USE_WEPOLL
