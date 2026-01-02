/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_WEPOLL_HPP_INCLUDED
#define SERVERLINK_WEPOLL_HPP_INCLUDED

#include "../util/config.hpp"

#if defined SL_USE_WEPOLL

#include <vector>
#include <map>
#include <winsock2.h>
#include <windows.h>

#include "fd.hpp"
#include "poller_base.hpp"
#include "../util/macros.hpp"

namespace slk
{
struct i_poll_events;
class ctx_t;

// Windows-optimized socket polling mechanism using WSAEventSelect
// This provides much better performance than plain select() on Windows
// by using event objects and WaitForMultipleObjects for efficient waiting.
//
// Key advantages over select on Windows:
// - No FD_SETSIZE limitation (can handle more than 64 sockets)
// - More efficient event notification through Windows event objects
// - Better scalability for high socket counts
//
// Note: Still limited to MAXIMUM_WAIT_OBJECTS (64) events per wait,
// but handles this by batching socket groups when needed.

class wepoll_t final : public worker_poller_base_t
{
  public:
    typedef void *handle_t;

    wepoll_t (ctx_t *ctx_);
    ~wepoll_t () override;

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

    // Socket entry storing fd, events, and associated Windows event object
    struct poll_entry_t
    {
        fd_t fd;
        WSAEVENT event;           // Windows event object for this socket
        slk::i_poll_events *events;
        bool pollin;              // Monitoring for read events
        bool pollout;             // Monitoring for write events
    };

    // Update WSAEventSelect for a socket based on pollin/pollout flags
    void update_socket_events (poll_entry_t *pe_);

    // Process events for sockets that were signaled
    void process_events (const std::vector<poll_entry_t *> &signaled_entries_);

    // List of all registered poll entries
    typedef std::vector<poll_entry_t *> poll_entries_t;
    poll_entries_t _entries;

    // List of retired event sources (waiting to be cleaned up)
    typedef std::vector<poll_entry_t *> retired_t;
    retired_t _retired;

    SL_NON_COPYABLE_NOR_MOVABLE (wepoll_t)
};

typedef wepoll_t poller_t;
}

#endif // SL_USE_WEPOLL

#endif
