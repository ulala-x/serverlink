/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "io_object.hpp"
#include "io_thread.hpp"
#include "poller.hpp"
#include "../util/err.hpp"

slk::io_object_t::io_object_t (io_thread_t *io_thread_) : _poller (NULL)
{
    if (io_thread_)
        plug (io_thread_);
}

slk::io_object_t::~io_object_t ()
{
}

void slk::io_object_t::plug (io_thread_t *io_thread_)
{
    slk_assert (io_thread_);
    slk_assert (!_poller);

    // Retrieve the poller from the thread we are running in
    _poller = static_cast<void*>(io_thread_->get_poller ());
}

void slk::io_object_t::unplug ()
{
    slk_assert (_poller);

    // Forget about old poller in preparation to be migrated
    // to a different I/O thread
    _poller = NULL;
}

slk::io_object_t::handle_t slk::io_object_t::add_fd (fd_t fd_)
{
    return static_cast<poller_t*>(_poller)->add_fd (fd_, this);
}

void slk::io_object_t::rm_fd (handle_t handle_)
{
    static_cast<poller_t*>(_poller)->rm_fd (handle_);
}

void slk::io_object_t::set_pollin (handle_t handle_)
{
    static_cast<poller_t*>(_poller)->set_pollin (handle_);
}

void slk::io_object_t::reset_pollin (handle_t handle_)
{
    static_cast<poller_t*>(_poller)->reset_pollin (handle_);
}

void slk::io_object_t::set_pollout (handle_t handle_)
{
    static_cast<poller_t*>(_poller)->set_pollout (handle_);
}

void slk::io_object_t::reset_pollout (handle_t handle_)
{
    static_cast<poller_t*>(_poller)->reset_pollout (handle_);
}

void slk::io_object_t::add_timer (int timeout_, int id_)
{
    static_cast<poller_t*>(_poller)->add_timer (timeout_, this, id_);
}

void slk::io_object_t::cancel_timer (int id_)
{
    static_cast<poller_t*>(_poller)->cancel_timer (this, id_);
}

void slk::io_object_t::in_event ()
{
    slk_assert (false);
}

void slk::io_object_t::out_event ()
{
    slk_assert (false);
}

void slk::io_object_t::timer_event (int)
{
    slk_assert (false);
}