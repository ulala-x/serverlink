/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_SELECT_HPP_INCLUDED
#define SERVERLINK_SELECT_HPP_INCLUDED

#include "../util/config.hpp"

#if defined SL_USE_SELECT

#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "fd.hpp"
#include "poller_base.hpp"
#include "../util/macros.hpp"

namespace slk
{
struct i_poll_events;
class ctx_t;

// Socket polling mechanism using select (cross-platform fallback)
// Note: select has limitations (FD_SETSIZE) but provides maximum portability

class select_t final : public worker_poller_base_t
{
  public:
    typedef fd_t handle_t;  // select uses fd directly as handle

    select_t (ctx_t *ctx_);
    ~select_t () override;

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

    // Helper method to update max_fd
    void update_max_fd ();

    struct fd_entry_t
    {
        fd_t fd;
        slk::i_poll_events *events;
        bool flag_pollin;
        bool flag_pollout;
    };

    typedef std::vector<fd_entry_t> fd_entries_t;
    fd_entries_t _fds;

    // fd_set structures for select
    // These are the "source" sets that we copy from for each select call
    fd_set _source_set_in;
    fd_set _source_set_out;
    fd_set _source_set_err;

    // List of retired file descriptors waiting to be removed
    typedef std::vector<fd_t> retired_t;
    retired_t _retired;

    // Maximum file descriptor value (POSIX only, ignored on Windows)
    fd_t _max_fd;

    // Flag to indicate that max_fd needs to be recalculated
    bool _need_update_max_fd;

    SL_NON_COPYABLE_NOR_MOVABLE (select_t)
};

typedef select_t poller_t;
}

#endif // SL_USE_SELECT

#endif
