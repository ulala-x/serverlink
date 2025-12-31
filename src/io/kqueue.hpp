/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_KQUEUE_HPP_INCLUDED
#define SERVERLINK_KQUEUE_HPP_INCLUDED

#include "../util/config.hpp"

#if defined SL_USE_KQUEUE

#include <vector>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include "fd.hpp"
#include "poller_base.hpp"
#include "../util/macros.hpp"

namespace slk
{
struct i_poll_events;
class ctx_t;

// Socket polling mechanism using BSD/macOS kqueue

class kqueue_t SL_FINAL : public worker_poller_base_t
{
  public:
    typedef void *handle_t;

    kqueue_t (ctx_t *ctx_);
    ~kqueue_t () SL_OVERRIDE;

    // "poller" concept
    handle_t add_fd (fd_t fd_, slk::i_poll_events *events_);
    void rm_fd (handle_t handle_);
    void set_pollin (handle_t handle_);
    void reset_pollin (handle_t handle_);
    void set_pollout (handle_t handle_);
    void reset_pollout (handle_t handle_);
    void stop ();

    static int max_fds ();

  private:
    // Main event loop
    void loop () SL_OVERRIDE;

    // Helper methods for kevent manipulation
    void kevent_add (fd_t fd_, short filter_, void *udata_);
    void kevent_delete (fd_t fd_, short filter_);

    // Main kqueue file descriptor
    fd_t _kqueue_fd;

    struct poll_entry_t
    {
        fd_t fd;
        bool flag_pollin;
        bool flag_pollout;
        slk::i_poll_events *events;
    };

    // List of retired event sources
    typedef std::vector<poll_entry_t *> retired_t;
    retired_t _retired;

    SL_NON_COPYABLE_NOR_MOVABLE (kqueue_t)
};

typedef kqueue_t poller_t;
}

#endif // SL_USE_KQUEUE

#endif
