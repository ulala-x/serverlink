/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_EPOLL_HPP_INCLUDED
#define SERVERLINK_EPOLL_HPP_INCLUDED

#include "../util/config.hpp"

#if defined SL_USE_EPOLL

#include <vector>
#include <sys/epoll.h>

#include "fd.hpp"
#include "poller_base.hpp"
#include "../util/macros.hpp"

namespace slk
{
struct i_poll_events;
class ctx_t;

// Socket polling mechanism using Linux epoll

class epoll_t final : public worker_poller_base_t
{
  public:
    typedef void *handle_t;

    epoll_t (ctx_t *ctx_);
    ~epoll_t () override;

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
    void loop () override;

    // Main epoll file descriptor
    fd_t _epoll_fd;

    struct poll_entry_t
    {
        fd_t fd;
        epoll_event ev;
        slk::i_poll_events *events;
    };

    // List of retired event sources
    typedef std::vector<poll_entry_t *> retired_t;
    retired_t _retired;

    SL_NON_COPYABLE_NOR_MOVABLE (epoll_t)
};

typedef epoll_t poller_t;
}

#endif // SL_USE_EPOLL

#endif
