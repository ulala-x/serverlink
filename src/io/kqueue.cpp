/* SPDX-License-Identifier: MPL-2.0 */

#include "../util/config.hpp"
#if defined SL_USE_KQUEUE

#include "kqueue.hpp"
#include "i_poll_events.hpp"
#include "../util/err.hpp"
#include "../util/macros.hpp"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <new>

namespace slk
{
kqueue_t::kqueue_t (ctx_t *ctx_) : worker_poller_base_t (ctx_)
{
    // Create kqueue file descriptor
    _kqueue_fd = kqueue ();
    errno_assert (_kqueue_fd != retired_fd);
}

kqueue_t::~kqueue_t ()
{
    // Wait till the worker thread exits
    stop_worker ();

    close (_kqueue_fd);

    for (retired_t::iterator it = _retired.begin (), end = _retired.end ();
         it != end; ++it) {
        delete *it;
    }
}

kqueue_t::handle_t kqueue_t::add_fd (fd_t fd_, i_poll_events *events_)
{
    check_thread ();
    poll_entry_t *pe = new (std::nothrow) poll_entry_t;
    alloc_assert (pe);

    // C++20: Use designated initializers for clear, efficient initialization
    *pe = poll_entry_t{
        .fd = fd_,
        .flag_pollin = false,
        .flag_pollout = false,
        .events = events_
    };

    // Increase the load metric of the thread
    adjust_load (1);

    return pe;
}

void kqueue_t::rm_fd (handle_t handle_)
{
    check_thread ();
    poll_entry_t *pe = static_cast<poll_entry_t *> (handle_);

    // Remove both read and write filters if they were set
    if (pe->flag_pollin)
        kevent_delete (pe->fd, EVFILT_READ);
    if (pe->flag_pollout)
        kevent_delete (pe->fd, EVFILT_WRITE);

    pe->fd = retired_fd;
    _retired.push_back (pe);

    // Decrease the load metric of the thread
    adjust_load (-1);
}

void kqueue_t::set_pollin (handle_t handle_)
{
    check_thread ();
    poll_entry_t *pe = static_cast<poll_entry_t *> (handle_);
    if (!pe->flag_pollin) {
        pe->flag_pollin = true;
        kevent_add (pe->fd, EVFILT_READ, pe);
    }
}

void kqueue_t::reset_pollin (handle_t handle_)
{
    check_thread ();
    poll_entry_t *pe = static_cast<poll_entry_t *> (handle_);
    if (pe->flag_pollin) {
        pe->flag_pollin = false;
        kevent_delete (pe->fd, EVFILT_READ);
    }
}

void kqueue_t::set_pollout (handle_t handle_)
{
    check_thread ();
    poll_entry_t *pe = static_cast<poll_entry_t *> (handle_);
    if (!pe->flag_pollout) {
        pe->flag_pollout = true;
        kevent_add (pe->fd, EVFILT_WRITE, pe);
    }
}

void kqueue_t::reset_pollout (handle_t handle_)
{
    check_thread ();
    poll_entry_t *pe = static_cast<poll_entry_t *> (handle_);
    if (pe->flag_pollout) {
        pe->flag_pollout = false;
        kevent_delete (pe->fd, EVFILT_WRITE);
    }
}

void kqueue_t::stop ()
{
    check_thread ();
    _stopping = true;
}

int kqueue_t::max_fds ()
{
    return -1;
}

void kqueue_t::kevent_add (fd_t fd_, short filter_, void *udata_)
{
    struct kevent ev;
    EV_SET (&ev, fd_, filter_, EV_ADD, 0, 0, udata_);
    const int rc = kevent (_kqueue_fd, &ev, 1, NULL, 0, NULL);
    errno_assert (rc != -1);
}

void kqueue_t::kevent_delete (fd_t fd_, short filter_)
{
    struct kevent ev;
    EV_SET (&ev, fd_, filter_, EV_DELETE, 0, 0, NULL);
    // Note: We ignore the return value here because the FD might have already
    // been closed, which would make EV_DELETE fail. This is expected behavior.
    kevent (_kqueue_fd, &ev, 1, NULL, 0, NULL);
}

void kqueue_t::loop ()
{
    struct kevent ev_buf[max_io_events];

    while (!_stopping) {
        // Execute any due timers
        const int timeout = static_cast<int> (execute_timers ());

        if (get_load () == 0) {
            if (timeout == 0)
                break;
            continue;
        }

        // Convert timeout to timespec
        struct timespec ts;
        struct timespec *timeout_ptr;
        if (timeout > 0) {
            ts.tv_sec = timeout / 1000;
            ts.tv_nsec = (timeout % 1000) * 1000000;
            timeout_ptr = &ts;
        } else {
            // timeout == 0 means no timers, wait indefinitely
            timeout_ptr = NULL;
        }

        // Wait for events
        const int n = kevent (_kqueue_fd, NULL, 0, &ev_buf[0], max_io_events,
                              timeout_ptr);
        if (n == -1) {
            errno_assert (errno == EINTR);
            continue;
        }

        for (int i = 0; i < n; i++) {
            poll_entry_t *pe = static_cast<poll_entry_t *> (ev_buf[i].udata);

            if (NULL == pe)
                continue;
            if (NULL == pe->events)
                continue;
            if (pe->fd == retired_fd)
                continue;

            // Handle error conditions (EV_EOF, EV_ERROR)
            // EV_EOF indicates the read/write end has been closed
            if (ev_buf[i].flags & EV_EOF)
                pe->events->in_event ();
            if (pe->fd == retired_fd)
                continue;

            // Handle error flag
            if (ev_buf[i].flags & EV_ERROR)
                pe->events->in_event ();
            if (pe->fd == retired_fd)
                continue;

            // Handle write events
            if (ev_buf[i].filter == EVFILT_WRITE)
                pe->events->out_event ();
            if (pe->fd == retired_fd)
                continue;

            // Handle read events
            if (ev_buf[i].filter == EVFILT_READ)
                pe->events->in_event ();
        }

        // Destroy retired event sources
        for (retired_t::iterator it = _retired.begin (), end = _retired.end ();
             it != end; ++it) {
            delete *it;
        }
        _retired.clear ();
    }
}

} // namespace slk

#endif // SL_USE_KQUEUE
