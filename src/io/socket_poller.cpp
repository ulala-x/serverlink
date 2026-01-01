/* ServerLink - High-performance message routing library */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../precompiled.hpp"
#include "socket_poller.hpp"
#include "../util/err.hpp"
#include "../io/polling_util.hpp"
#include "../util/macros.hpp"
#include "../util/clock.hpp"
#include "../util/constants.hpp"

#include <limits.h>

// Poll event constants - these match the public API defines
static const short SLK_POLLIN = slk::SL_POLLIN;
static const short SLK_POLLOUT = slk::SL_POLLOUT;
static const short SLK_POLLERR = slk::SL_POLLERR;

static bool is_thread_safe (const slk::socket_base_t &socket_)
{
    // Thread-safe sockets (CLIENT/SERVER) are not yet implemented in ServerLink
    // For now, all sockets are non-thread-safe
    return false;
}

// Compare elements to value
template <class It, class T, class Pred>
static It find_if2 (It b_, It e_, const T &value, Pred pred)
{
    for (; b_ != e_; ++b_) {
        if (pred (*b_, value)) {
            break;
        }
    }
    return b_;
}

slk::socket_poller_t::socket_poller_t () :
    _tag (0xCAFEBABE),
    _signaler (NULL)
#if defined SL_POLL_BASED_ON_POLL
    ,
    _pollfds (NULL)
#elif defined SL_POLL_BASED_ON_SELECT
    ,
    _max_fd (0)
#endif
{
    rebuild ();
}

slk::socket_poller_t::~socket_poller_t ()
{
    // Mark the socket_poller as dead
    _tag = 0xdeadbeef;

    for (items_t::iterator it = _items.begin (), end = _items.end (); it != end;
         ++it) {
        if (it->socket && is_thread_safe (*it->socket)) {
            it->socket->remove_signaler (_signaler);
        }
    }

    if (_signaler != NULL) {
        delete _signaler;
    }

#if defined SL_POLL_BASED_ON_POLL
    if (_pollfds) {
        free (_pollfds);
        _pollfds = NULL;
    }
#endif
}

bool slk::socket_poller_t::check_tag () const
{
    return _tag == 0xCAFEBABE;
}

int slk::socket_poller_t::signaler_fd (fd_t *fd_) const
{
    if (_signaler) {
        *fd_ = _signaler->get_fd ();
        return 0;
    }
    // Only thread-safe socket types are guaranteed to have a signaler.
    errno = EINVAL;
    return -1;
}

int slk::socket_poller_t::add (socket_base_t *socket_,
                               void *user_data_,
                               short events_)
{
    if (find_if2 (_items.begin (), _items.end (), socket_, &is_socket)
        != _items.end ()) {
        errno = EINVAL;
        return -1;
    }

    if (is_thread_safe (*socket_)) {
        if (_signaler == NULL) {
            _signaler = new (std::nothrow) signaler_t ();
            if (!_signaler) {
                errno = ENOMEM;
                return -1;
            }
            if (!_signaler->valid ()) {
                delete _signaler;
                _signaler = NULL;
                errno = EMFILE;
                return -1;
            }
        }

        socket_->add_signaler (_signaler);
    }

    const item_t item = {
        socket_,
        0,
        user_data_,
        events_
#if defined SL_POLL_BASED_ON_POLL
        ,
        -1
#endif
    };
    try {
        _items.push_back (item);
    }
    catch (const std::bad_alloc &) {
        errno = ENOMEM;
        return -1;
    }
    _need_rebuild = true;

    return 0;
}

int slk::socket_poller_t::add_fd (fd_t fd_, void *user_data_, short events_)
{
    if (find_if2 (_items.begin (), _items.end (), fd_, &is_fd)
        != _items.end ()) {
        errno = EINVAL;
        return -1;
    }

    const item_t item = {
        NULL,
        fd_,
        user_data_,
        events_
#if defined SL_POLL_BASED_ON_POLL
        ,
        -1
#endif
    };
    try {
        _items.push_back (item);
    }
    catch (const std::bad_alloc &) {
        errno = ENOMEM;
        return -1;
    }
    _need_rebuild = true;

    return 0;
}

int slk::socket_poller_t::modify (const socket_base_t *socket_, short events_)
{
    const items_t::iterator it =
      find_if2 (_items.begin (), _items.end (), socket_, &is_socket);

    if (it == _items.end ()) {
        errno = EINVAL;
        return -1;
    }

    it->events = events_;
    _need_rebuild = true;

    return 0;
}


int slk::socket_poller_t::modify_fd (fd_t fd_, short events_)
{
    const items_t::iterator it =
      find_if2 (_items.begin (), _items.end (), fd_, &is_fd);

    if (it == _items.end ()) {
        errno = EINVAL;
        return -1;
    }

    it->events = events_;
    _need_rebuild = true;

    return 0;
}


int slk::socket_poller_t::remove (socket_base_t *socket_)
{
    const items_t::iterator it =
      find_if2 (_items.begin (), _items.end (), socket_, &is_socket);

    if (it == _items.end ()) {
        errno = EINVAL;
        return -1;
    }

    _items.erase (it);
    _need_rebuild = true;

    if (is_thread_safe (*socket_)) {
        socket_->remove_signaler (_signaler);
    }

    return 0;
}

int slk::socket_poller_t::remove_fd (fd_t fd_)
{
    const items_t::iterator it =
      find_if2 (_items.begin (), _items.end (), fd_, &is_fd);

    if (it == _items.end ()) {
        errno = EINVAL;
        return -1;
    }

    _items.erase (it);
    _need_rebuild = true;

    return 0;
}

int slk::socket_poller_t::rebuild ()
{
    _use_signaler = false;
    _pollset_size = 0;
    _need_rebuild = false;

#if defined SL_POLL_BASED_ON_POLL

    if (_pollfds) {
        free (_pollfds);
        _pollfds = NULL;
    }

    for (items_t::iterator it = _items.begin (), end = _items.end (); it != end;
         ++it) {
        if (it->events) {
            if (it->socket && is_thread_safe (*it->socket)) {
                if (!_use_signaler) {
                    _use_signaler = true;
                    _pollset_size++;
                }
            } else
                _pollset_size++;
        }
    }

    if (_pollset_size == 0)
        return 0;

    _pollfds = static_cast<pollfd *> (malloc (_pollset_size * sizeof (pollfd)));

    if (!_pollfds) {
        errno = ENOMEM;
        _need_rebuild = true;
        return -1;
    }

    int item_nbr = 0;

    if (_use_signaler) {
        item_nbr = 1;
        _pollfds[0].fd = _signaler->get_fd ();
        _pollfds[0].events = POLLIN;
    }

    for (items_t::iterator it = _items.begin (), end = _items.end (); it != end;
         ++it) {
        if (it->events) {
            if (it->socket) {
                if (!is_thread_safe (*it->socket)) {
                    size_t fd_size = sizeof (slk::fd_t);
                    slk::fd_t notify_fd;
                    const int rc = it->socket->getsockopt (
                      SL_FD, &notify_fd, &fd_size);
                    slk_assert (rc == 0);

                    _pollfds[item_nbr].fd = notify_fd;
                    _pollfds[item_nbr].events = POLLIN;
                    item_nbr++;
                }
            } else {
                _pollfds[item_nbr].fd = it->fd;
                _pollfds[item_nbr].events =
                  (it->events & SLK_POLLIN ? POLLIN : 0)
                  | (it->events & SLK_POLLOUT ? POLLOUT : 0)
                  | (it->events & SLK_POLLERR ? POLLPRI : 0);
                it->pollfd_index = item_nbr;
                item_nbr++;
            }
        }
    }

#elif defined SL_POLL_BASED_ON_SELECT

    // Ensure we do not attempt to select () on more than FD_SETSIZE
    // file descriptors.
    slk_assert (_items.size () <= FD_SETSIZE);

    _pollset_in.resize (_items.size ());
    _pollset_out.resize (_items.size ());
    _pollset_err.resize (_items.size ());

    FD_ZERO (_pollset_in.get ());
    FD_ZERO (_pollset_out.get ());
    FD_ZERO (_pollset_err.get ());

    for (items_t::iterator it = _items.begin (), end = _items.end (); it != end;
         ++it) {
        if (it->socket && is_thread_safe (*it->socket) && it->events) {
            _use_signaler = true;
            FD_SET (_signaler->get_fd (), _pollset_in.get ());
            _pollset_size = 1;
            break;
        }
    }

    _max_fd = 0;

    // Build the fd_sets for passing to select ().
    for (items_t::iterator it = _items.begin (), end = _items.end (); it != end;
         ++it) {
        if (it->events) {
            // If the poll item is a ServerLink socket we are interested in input on the
            // notification file descriptor retrieved by the SL_FD socket option.
            if (it->socket) {
                if (!is_thread_safe (*it->socket)) {
                    slk::fd_t notify_fd;
                    size_t fd_size = sizeof (slk::fd_t);
                    int rc =
                      it->socket->getsockopt (SL_FD, &notify_fd, &fd_size);
                    slk_assert (rc == 0);

                    FD_SET (notify_fd, _pollset_in.get ());
                    if (_max_fd < notify_fd)
                        _max_fd = notify_fd;

                    _pollset_size++;
                }
            }
            // Else, the poll item is a raw file descriptor. Convert the poll item
            // events to the appropriate fd_sets.
            else {
                if (it->events & SLK_POLLIN)
                    FD_SET (it->fd, _pollset_in.get ());
                if (it->events & SLK_POLLOUT)
                    FD_SET (it->fd, _pollset_out.get ());
                if (it->events & SLK_POLLERR)
                    FD_SET (it->fd, _pollset_err.get ());
                if (_max_fd < it->fd)
                    _max_fd = it->fd;

                _pollset_size++;
            }
        }
    }

#endif

    return 0;
}

void slk::socket_poller_t::zero_trail_events (
  slk::socket_poller_t::event_t *events_, int n_events_, int found_)
{
    for (int i = found_; i < n_events_; ++i) {
        events_[i].socket = NULL;
        events_[i].fd = slk::retired_fd;
        events_[i].user_data = NULL;
        events_[i].events = 0;
    }
}

#if defined SL_POLL_BASED_ON_POLL
int slk::socket_poller_t::check_events (slk::socket_poller_t::event_t *events_,
                                        int n_events_)
#elif defined SL_POLL_BASED_ON_SELECT
int slk::socket_poller_t::check_events (slk::socket_poller_t::event_t *events_,
                                        int n_events_,
                                        fd_set &inset_,
                                        fd_set &outset_,
                                        fd_set &errset_)
#endif
{
    int found = 0;
    for (items_t::iterator it = _items.begin (), end = _items.end ();
         it != end && found < n_events_; ++it) {
        // The poll item is a ServerLink socket. Retrieve pending events
        // using the SL_EVENTS socket option.
        if (it->socket) {
            size_t events_size = sizeof (uint32_t);
            uint32_t events;
            if (it->socket->getsockopt (SL_EVENTS, &events, &events_size)
                == -1) {
                return -1;
            }

            if (it->events & events) {
                events_[found].socket = it->socket;
                events_[found].fd = slk::retired_fd;
                events_[found].user_data = it->user_data;
                events_[found].events = it->events & events;
                ++found;
            }
        }
        // Else, the poll item is a raw file descriptor, simply convert
        // the events to slk_pollitem_t-style format.
        else if (it->events) {
#if defined SL_POLL_BASED_ON_POLL
            slk_assert (it->pollfd_index >= 0);
            const short revents = _pollfds[it->pollfd_index].revents;
            short events = 0;

            if (revents & POLLIN)
                events |= SLK_POLLIN;
            if (revents & POLLOUT)
                events |= SLK_POLLOUT;
            if (revents & POLLPRI)
                events |= SLK_POLLERR;
            if (revents & ~(POLLIN | POLLOUT | POLLPRI))
                events |= SLK_POLLERR;

#elif defined SL_POLL_BASED_ON_SELECT

            short events = 0;

            if (FD_ISSET (it->fd, &inset_))
                events |= SLK_POLLIN;
            if (FD_ISSET (it->fd, &outset_))
                events |= SLK_POLLOUT;
            if (FD_ISSET (it->fd, &errset_))
                events |= SLK_POLLERR;
#endif //POLL_SELECT

            if (events) {
                events_[found].socket = NULL;
                events_[found].fd = it->fd;
                events_[found].user_data = it->user_data;
                events_[found].events = events;
                ++found;
            }
        }
    }

    return found;
}

// Return 0 if timeout is expired otherwise 1
int slk::socket_poller_t::adjust_timeout (slk::clock_t &clock_,
                                          long timeout_,
                                          uint64_t &now_,
                                          uint64_t &end_,
                                          bool &first_pass_)
{
    // If socket_poller_t::timeout is zero, exit immediately whether there
    // are events or not.
    if (timeout_ == 0)
        return 0;

    // At this point we are meant to wait for events but there are none.
    // If timeout is infinite we can just loop until we get some events.
    if (timeout_ < 0) {
        if (first_pass_)
            first_pass_ = false;
        return 1;
    }

    // The timeout is finite and there are no events. In the first pass
    // we get a timestamp of when the polling have begun. (We assume that
    // first pass have taken negligible time). We also compute the time
    // when the polling should time out.
    now_ = clock_.now_ms ();
    if (first_pass_) {
        end_ = now_ + timeout_;
        first_pass_ = false;
        return 1;
    }

    // Find out whether timeout have expired.
    if (now_ >= end_)
        return 0;

    return 1;
}

int slk::socket_poller_t::wait (slk::socket_poller_t::event_t *events_,
                                int n_events_,
                                long timeout_)
{
    if (_items.empty () && timeout_ < 0) {
        errno = EFAULT;
        return -1;
    }

    if (_need_rebuild) {
        const int rc = rebuild ();
        if (rc == -1)
            return -1;
    }

    if (unlikely (_pollset_size == 0)) {
        if (timeout_ < 0) {
            // Fail instead of trying to sleep forever
            errno = EFAULT;
            return -1;
        }
        // We'll report an error (timed out) as if the list was non-empty and
        // no event occurred within the specified timeout. Otherwise the caller
        // needs to check the return value AND the event to avoid using the
        // nullified event data.
        errno = EAGAIN;
        if (timeout_ == 0)
            return -1;
#if defined SL_HAVE_WINDOWS
        Sleep (timeout_ > 0 ? timeout_ : INFINITE);
        return -1;
#elif defined SL_HAVE_ANDROID
        usleep (timeout_ * 1000);
        return -1;
#elif defined SL_HAVE_OSX
        usleep (timeout_ * 1000);
        errno = EAGAIN;
        return -1;
#elif defined SL_HAVE_VXWORKS
        struct timespec ns_;
        ns_.tv_sec = timeout_ / 1000;
        ns_.tv_nsec = timeout_ % 1000 * 1000000;
        nanosleep (&ns_, 0);
        return -1;
#else
        usleep (timeout_ * 1000);
        return -1;
#endif
    }

#if defined SL_POLL_BASED_ON_POLL
    slk::clock_t clock;
    uint64_t now = 0;
    uint64_t end = 0;

    bool first_pass = true;

    while (true) {
        // Compute the timeout for the subsequent poll.
        int timeout;
        if (first_pass)
            timeout = 0;
        else if (timeout_ < 0)
            timeout = -1;
        else
            timeout =
              static_cast<int> (std::min<uint64_t> (end - now, INT_MAX));

        // Wait for events.
        const int rc = poll (_pollfds, _pollset_size, timeout);
        if (rc == -1 && errno == EINTR) {
            return -1;
        }
        errno_assert (rc >= 0);

        // Receive the signal from pollfd
        if (_use_signaler && _pollfds[0].revents & POLLIN)
            _signaler->recv ();

        // Check for the events.
        const int found = check_events (events_, n_events_);
        if (found) {
            if (found > 0)
                zero_trail_events (events_, n_events_, found);
            return found;
        }

        // Adjust timeout or break
        if (adjust_timeout (clock, timeout_, now, end, first_pass) == 0)
            break;
    }
    errno = EAGAIN;
    return -1;

#elif defined SL_POLL_BASED_ON_SELECT

    slk::clock_t clock;
    uint64_t now = 0;
    uint64_t end = 0;

    bool first_pass = true;

    optimized_fd_set_t inset (_pollset_size);
    optimized_fd_set_t outset (_pollset_size);
    optimized_fd_set_t errset (_pollset_size);

    while (true) {
        // Compute the timeout for the subsequent poll.
        timeval timeout;
        timeval *ptimeout;
        if (first_pass) {
            timeout.tv_sec = 0;
            timeout.tv_usec = 0;
            ptimeout = &timeout;
        } else if (timeout_ < 0)
            ptimeout = NULL;
        else {
            timeout.tv_sec = static_cast<long> ((end - now) / 1000);
            timeout.tv_usec = static_cast<long> ((end - now) % 1000 * 1000);
            ptimeout = &timeout;
        }

        // Wait for events. Ignore interrupts if there's infinite timeout.
        memcpy (inset.get (), _pollset_in.get (),
                valid_pollset_bytes (*_pollset_in.get ()));
        memcpy (outset.get (), _pollset_out.get (),
                valid_pollset_bytes (*_pollset_out.get ()));
        memcpy (errset.get (), _pollset_err.get (),
                valid_pollset_bytes (*_pollset_err.get ()));
        const int rc = select (static_cast<int> (_max_fd + 1), inset.get (),
                               outset.get (), errset.get (), ptimeout);
#if defined SL_HAVE_WINDOWS
        if (unlikely (rc == SOCKET_ERROR)) {
            errno = wsa_error_to_errno (WSAGetLastError ());
            wsa_assert (errno == ENOTSOCK);
            return -1;
        }
#else
        if (unlikely (rc == -1)) {
            errno_assert (errno == EINTR || errno == EBADF);
            return -1;
        }
#endif

        if (_use_signaler && FD_ISSET (_signaler->get_fd (), inset.get ()))
            _signaler->recv ();

        // Check for the events.
        const int found = check_events (events_, n_events_, *inset.get (),
                                        *outset.get (), *errset.get ());
        if (found) {
            if (found > 0)
                zero_trail_events (events_, n_events_, found);
            return found;
        }

        // Adjust timeout or break
        if (adjust_timeout (clock, timeout_, now, end, first_pass) == 0)
            break;
    }

    errno = EAGAIN;
    return -1;

#else

    // Exotic platforms that support neither poll() nor select().
    errno = ENOTSUP;
    return -1;

#endif
}
