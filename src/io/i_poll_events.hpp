/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_I_POLL_EVENTS_HPP_INCLUDED
#define SERVERLINK_I_POLL_EVENTS_HPP_INCLUDED

#include "../util/macros.hpp"
#include "../util/config.hpp"
#include "fd.hpp"

namespace slk
{
// Virtual interface to be exposed by objects that want to be notified
// about events on file descriptors.

struct i_poll_events
{
    virtual ~i_poll_events () = default;

    // =========================================================================
    // Reactor Pattern (select/epoll/kqueue) - Readiness-based notifications
    // =========================================================================

    // Called by I/O thread when file descriptor is ready for reading
    virtual void in_event () = 0;

    // Called by I/O thread when file descriptor is ready for writing
    virtual void out_event () = 0;

    // Called when timer expires
    virtual void timer_event (int id_) = 0;

#ifdef SL_USE_IOCP
    // =========================================================================
    // Proactor Pattern (IOCP) - Completion-based notifications with data
    // =========================================================================

    // Called when async read operation completes with received data
    // Parameters:
    //   data_: pointer to received data buffer (valid only during call)
    //   size_: number of bytes received (0 indicates connection closed)
    //   error_: Windows error code (0 = success)
    // Default implementation: forwards to in_event() for backward compatibility
    virtual void in_completed (const void *data_, size_t size_, int error_)
    {
        // Default implementation maintains backward compatibility
        // Derived classes can override for Direct Engine optimization
        if (error_ == 0) {
            in_event ();
        }
    }

    // Called when async write operation completes
    // Parameters:
    //   bytes_sent_: number of bytes actually sent
    //   error_: Windows error code (0 = success)
    // Default implementation: forwards to out_event() for backward compatibility
    virtual void out_completed (size_t bytes_sent_, int error_)
    {
        // Default implementation maintains backward compatibility
        // Derived classes can override for Direct Engine optimization
        if (error_ == 0) {
            out_event ();
        }
    }

    // Called when AcceptEx operation completes with new connection
    // Parameters:
    //   accept_fd_: accepted socket descriptor (already has SO_UPDATE_ACCEPT_CONTEXT)
    //   error_: Windows error code (0 = success)
    // Default implementation: forwards to in_event() for backward compatibility
    virtual void accept_completed (fd_t accept_fd_, int error_)
    {
        // Default: fallback to traditional accept() via in_event()
        // Listener classes should override to use accept_fd_ directly
        if (error_ == 0) {
            in_event ();
        }
    }

    // Called when ConnectEx operation completes
    // Parameters:
    //   error_: Windows error code (0 = success)
    // Default implementation: forwards to out_event() for backward compatibility
    virtual void connect_completed (int error_)
    {
        // Default: forward to out_event() on success
        // Connecter classes should override for direct handling
        if (error_ == 0) {
            out_event ();
        }
    }
#endif
};
}

#endif
