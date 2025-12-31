/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_I_POLL_EVENTS_HPP_INCLUDED
#define SERVERLINK_I_POLL_EVENTS_HPP_INCLUDED

#include "../util/macros.hpp"

namespace slk
{
// Virtual interface to be exposed by objects that want to be notified
// about events on file descriptors.

struct i_poll_events
{
    virtual ~i_poll_events () SL_DEFAULT;

    // Called by I/O thread when file descriptor is ready for reading
    virtual void in_event () = 0;

    // Called by I/O thread when file descriptor is ready for writing
    virtual void out_event () = 0;

    // Called when timer expires
    virtual void timer_event (int id_) = 0;
};
}

#endif
